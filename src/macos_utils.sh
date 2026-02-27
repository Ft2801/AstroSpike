
# =============================================================================
# AstroSpike macOS Build Utilities
# Shared functions for build_macos.sh, package_macos.sh, build_installer_macos.sh
# =============================================================================

# --- Version Management ---
get_version() {
    local VERSION="1.0.0"
    if [ -f "changelog.txt" ]; then
        VERSION=$(grep -E "^Version [0-9.]+" changelog.txt | head -1 | awk '{print $2}' | tr -d '\r')
        if [ -z "$VERSION" ]; then
            VERSION="1.0.0"
        fi
    fi
    echo "$VERSION"
}

# --- Homebrew Utilities ---
get_homebrew_prefix() {
    local prefix=$(brew --prefix 2>/dev/null || echo "")
    if [ -z "$prefix" ]; then
        # Fallback for ARM64
        if [ -d "/opt/homebrew" ]; then
            prefix="/opt/homebrew"
        # Fallback for Intel
        elif [ -d "/usr/local" ]; then
            prefix="/usr/local"
        fi
    fi
    echo "$prefix"
}

# Detect if running under Rosetta (Intel on ARM)
is_rosetta() {
    local arch=$(arch 2>/dev/null || echo "unknown")
    if [ "$arch" == "i386" ]; then
        return 0  # Running under Rosetta
    fi
    return 1  # Native ARM64
}

# --- Qt Detection (Robust) ---
detect_qt_prefix() {
    local qt_prefix=""
    
    # Try qt@6 first (newer convention)
    qt_prefix=$(brew --prefix qt@6 2>/dev/null || echo "")
    
    # Fallback to qt
    if [ -z "$qt_prefix" ]; then
        qt_prefix=$(brew --prefix qt 2>/dev/null || echo "")
    fi
    
    # If symlink is broken, search Cellar
    if [ -z "$qt_prefix" ] || [ ! -d "$qt_prefix/bin" ]; then
        local cellar_qt=$(find /opt/homebrew/Cellar/qt* -maxdepth 2 -name "bin" -type d 2>/dev/null | head -1)
        if [ -n "$cellar_qt" ]; then
            qt_prefix=$(dirname "$cellar_qt")
        fi
    fi
    
    # Final fallback for Intel
    if [ -z "$qt_prefix" ] && [ -d "/usr/local/opt/qt@6" ]; then
        qt_prefix="/usr/local/opt/qt@6"
    fi
    
    echo "$qt_prefix"
}

# Find macdeployqt in Qt installation
find_macdeployqt() {
    local qt_prefix="$1"
    local macdeployqt=""
    
    if [ -f "$qt_prefix/bin/macdeployqt" ]; then
        macdeployqt="$qt_prefix/bin/macdeployqt"
    else
        # Try PATH
        macdeployqt=$(which macdeployqt 2>/dev/null || echo "")
    fi
    
    echo "$macdeployqt"
}

# --- Architecture Detection ---
detect_build_architecture() {
    local executable="$1"
    if [ -n "$executable" ] && [ -f "$executable" ]; then
        local file_output=$(file "$executable" 2>/dev/null || echo "")
        local host_arch=$(uname -m)
        
        if [ "$host_arch" == "arm64" ]; then
             if echo "$file_output" | grep -q "arm64"; then
                echo "arm64"
                return 0
            elif echo "$file_output" | grep -q "x86_64"; then
                echo "x86_64"
                return 0
            fi
        else
            if echo "$file_output" | grep -q "x86_64"; then
                echo "x86_64"
                return 0
            elif echo "$file_output" | grep -q "arm64"; then
                echo "arm64"
                return 0
            fi
        fi
    fi
    
    # Fallback to native architecture
    local native_arch=$(uname -m)
    if [ "$native_arch" == "arm64" ] || [ "$native_arch" == "aarch64" ]; then
        echo "arm64"
    else
        echo "x86_64"
    fi
}

# Check if dylib has matching architecture
dylib_matches_arch() {
    local dylib="$1"
    local target_arch="$2"
    
    if [ ! -f "$dylib" ]; then
        return 1
    fi
    
    local file_output=$(file "$dylib" 2>/dev/null || echo "")
    
    # PERMISSIVE CHECK: If it contains the target architecture, it's fine.
    # This allows Universal binaries (containing both archs).
    if echo "$file_output" | grep -q "$target_arch"; then
        return 0
    fi
    
    return 1
}

# Recursively copy all dependencies of a dylib
copy_dylib_with_dependencies() {
    local dylib="$1"
    local dest_dir="$2"
    local target_arch="$3"
    local processed_dylibs="${4:-}"
    
    # Avoid infinite loops
    if echo "$processed_dylibs" | grep -q "$(basename "$dylib")"; then
        return 0
    fi
    processed_dylibs="$processed_dylibs $(basename "$dylib")"
    
    # Get all dependencies
    local deps=$(otool -L "$dylib" 2>/dev/null | grep -v "^$dylib:" | grep "\.dylib" | awk '{print $1}' | sort -u || true)
    
    for dep in $deps; do
        # Skip system dylibs
        if echo "$dep" | grep -qE "^(/usr/lib|/System|@executable_path)"; then
            continue
        fi
        
        # Skip Qt frameworks (handled by macdeployqt)
        if echo "$dep" | grep -q "\.framework"; then
            continue
        fi
        
        # Resolve @rpath references: extract the basename and search for it
        local dep_basename
        if echo "$dep" | grep -q "@rpath"; then
            dep_basename=$(echo "$dep" | sed 's|@rpath/||')
        else
            dep_basename=$(basename "$dep")
        fi
        
        # Check if already bundled
        if [ -f "$dest_dir/$dep_basename" ]; then
            continue
        fi
        
        # Try to find and copy from Homebrew
        local found=0
        for brew_path in /opt/homebrew /usr/local; do
            # 1. Check standard lib dir
            if [ -f "$brew_path/lib/$dep_basename" ]; then
                if dylib_matches_arch "$brew_path/lib/$dep_basename" "$target_arch"; then
                    cp -L "$brew_path/lib/$dep_basename" "$dest_dir/" 2>/dev/null && found=1
                    if [ $found -eq 1 ]; then
                        copy_dylib_with_dependencies "$dest_dir/$dep_basename" "$dest_dir" "$target_arch" "$processed_dylibs" || true
                    fi
                fi
                break
            fi
            # 2. Search opt/*/lib/ (covers openblas, etc.)
            if [ $found -eq 0 ] && [ -d "$brew_path/opt" ]; then
                local opt_match=$(find -L "$brew_path/opt" -maxdepth 3 -name "$dep_basename" 2>/dev/null | head -1)
                if [ -n "$opt_match" ]; then
                    if dylib_matches_arch "$opt_match" "$target_arch"; then
                        cp -L "$opt_match" "$dest_dir/" 2>/dev/null && found=1
                        if [ $found -eq 1 ]; then
                            copy_dylib_with_dependencies "$dest_dir/$dep_basename" "$dest_dir" "$target_arch" "$processed_dylibs" || true
                        fi
                    fi
                fi
            fi
            # 3. Search Cellar directly for versioned paths
            if [ $found -eq 0 ] && [ -d "$brew_path/Cellar" ]; then
                local cellar_match=$(find -L "$brew_path/Cellar" -maxdepth 5 -name "$dep_basename" 2>/dev/null | head -1)
                if [ -n "$cellar_match" ]; then
                    if dylib_matches_arch "$cellar_match" "$target_arch"; then
                        cp -L "$cellar_match" "$dest_dir/" 2>/dev/null && found=1
                        if [ $found -eq 1 ]; then
                            copy_dylib_with_dependencies "$dest_dir/$dep_basename" "$dest_dir" "$target_arch" "$processed_dylibs" || true
                        fi
                    fi
                fi
            fi
        done
        
        # Also try the original absolute path directly (if it's not @rpath)
        if [ $found -eq 0 ] && ! echo "$dep" | grep -q "@rpath"; then
            if [ -f "$dep" ]; then
                if dylib_matches_arch "$dep" "$target_arch"; then
                    cp -L "$dep" "$dest_dir/" 2>/dev/null && found=1
                    if [ $found -eq 1 ]; then
                        copy_dylib_with_dependencies "$dest_dir/$dep_basename" "$dest_dir" "$target_arch" "$processed_dylibs" || true
                    fi
                fi
            fi
        fi
        
        if [ $found -eq 0 ] && ! echo "$dep" | grep -qE "libSystem\.B|libobjc\.A|libstdc|libc\+\+"; then
            true  # Silent skip for system libs
        fi
    done
}

# --- Dependency Utilities ---
copy_dylib() {
    local lib_name="$1"
    local brew_pkg="$2"
    local dest_dir="$3"
    local target_arch="${4:-}"
    
    # If no target arch specified, detect from the executable
    if [ -z "$target_arch" ]; then
        if [ -d "$dest_dir/../MacOS" ]; then
            local executable=$(find "$dest_dir/../MacOS" -name "AstroSpike" -o -name "*.app" 2>/dev/null | head -1)
            if [ -z "$executable" ]; then
                executable=$(find "$dest_dir/../MacOS" -type f -executable 2>/dev/null | head -1)
            fi
            if [ -n "$executable" ]; then
                target_arch=$(detect_build_architecture "$executable")
            fi
        fi
    fi
    
    # Fallback to native architecture
    if [ -z "$target_arch" ]; then
        target_arch=$(detect_build_architecture "")
    fi

    # Determine search paths based on target architecture priority
    local search_paths=()
    if [ "$target_arch" == "x86_64" ]; then
        search_paths=("/usr/local" "/opt/homebrew")
    else
        search_paths=("/opt/homebrew" "/usr/local")
    fi

    # Ask Homebrew directly for the prefix (MOST RELIABLE)
    local brew_prefix=$(brew --prefix "$brew_pkg" 2>/dev/null || echo "")
    if [ -n "$brew_prefix" ]; then
        search_paths=("$brew_prefix" "${search_paths[@]}")
    fi

    # Iterate through search paths
    local found_any_arch=""
    local checked_paths=""

    for base_path in "${search_paths[@]}"; do
        if [ ! -d "$base_path" ]; then continue; fi
        checked_paths="$checked_paths $base_path"

        local candidates=()
        
        # Method A: Standard library path
        if [ -d "$base_path/lib" ]; then
             local found=$(find -L "$base_path/lib" -name "${lib_name}*.dylib" -maxdepth 2 2>/dev/null | grep -v ".dSYM" | sort)
             if [ -n "$found" ]; then candidates+=($found); fi
        fi
        
        # Method B: Cellar fallback
        if [ -d "$base_path/Cellar/$brew_pkg" ]; then
             local found=$(find -L "$base_path/Cellar/$brew_pkg" -name "${lib_name}*.dylib" -maxdepth 6 2>/dev/null | grep -v ".dSYM" | sort)
             if [ -n "$found" ]; then candidates+=($found); fi
        fi

        # Method C: Aggressive search
        if [ -z "$found_any_arch" ] && [ ${#candidates[@]} -eq 0 ]; then
             local name_pattern="${lib_name}*.dylib"
             local found=$(find -L "$base_path" -maxdepth 8 -name "$name_pattern" 2>/dev/null | grep -v ".dSYM" | sort)
             if [ -n "$found" ]; then candidates+=($found); fi
        fi

        for dylib in "${candidates[@]}"; do
            if [ -f "$dylib" ]; then
                 if dylib_matches_arch "$dylib" "$target_arch"; then
                    cp -L "$dylib" "$dest_dir/${lib_name}.dylib" 2>/dev/null
                    
                    if [ "$(basename "$dylib")" != "${lib_name}.dylib" ]; then
                        cp -L "$dylib" "$dest_dir/" 2>/dev/null
                    fi
                    
                    echo "  - $lib_name: OK ($target_arch) [from: $base_path]"
                    return 0
                 else
                    local found_arch=$(file "$dylib" 2>/dev/null | grep -o "x86_64\|arm64" | head -1)
                    found_any_arch="$found_arch"
                 fi
            fi
        done
    done

    # If we get here, valid library was not found
    if [ -n "$found_any_arch" ]; then
        echo "  - $lib_name: ARCH MISMATCH (target: $target_arch, found: $found_any_arch in checked paths)"
        if [ "$target_arch" == "x86_64" ] && [ "$found_any_arch" == "arm64" ]; then
            echo "    [TIP] Try: arch -x86_64 brew install $brew_pkg"
        fi
    else
        echo "  - $lib_name: NOT FOUND in paths: $checked_paths"
        echo "    [TIP] If building for arm64, ensure 'brew install $brew_pkg' was run."
    fi
    return 1
}

# --- Library Fixup Utilities ---
fix_dylib_id_and_deps() {
    local dylib_path="$1"
    local frameworks_dir="$2"
    
    if [ ! -f "$dylib_path" ]; then
        return
    fi
    
    chmod +w "$dylib_path"
    
    local dylib_name=$(basename "$dylib_path")
    
    # 1. Set the ID to @rpath/...
    install_name_tool -id "@rpath/$dylib_name" "$dylib_path" 2>/dev/null || true
    
    # 2. Fix dependencies
    local deps=$(otool -L "$dylib_path" 2>/dev/null | grep -v "^$dylib_path:" | awk '{print $1}')
    
    for dep in $deps; do
        # Skip system paths and already-correct references
        if echo "$dep" | grep -qE "^(/usr/lib|/System)"; then
            continue
        fi
        if echo "$dep" | grep -q "@executable_path"; then
            continue
        fi
        
        local dep_name=$(basename "$dep")
        
        if [ -f "$frameworks_dir/$dep_name" ]; then
            if [ "$dep" != "@rpath/$dep_name" ]; then
                install_name_tool -change "$dep" "@rpath/$dep_name" "$dylib_path" 2>/dev/null || true
            fi
        elif echo "$dep" | grep -qE "^(/opt/homebrew|/usr/local/(Cellar|opt|lib))"; then
            install_name_tool -change "$dep" "@rpath/$dep_name" "$dylib_path" 2>/dev/null || true
        fi
    done
}

fix_executable_deps() {
    local exec_path="$1"
    local frameworks_dir="$2"
    
    if [ ! -f "$exec_path" ] || [ ! -d "$frameworks_dir" ]; then
        return
    fi
    
    chmod +w "$exec_path"
    
    local deps=$(otool -L "$exec_path" 2>/dev/null | grep -v "^$exec_path:" | awk '{print $1}')
    
    for dep in $deps; do
        # Skip system paths
        if echo "$dep" | grep -qE "^(/usr/lib|/System)"; then
            continue
        fi
        if echo "$dep" | grep -q "@executable_path"; then
            continue
        fi
        
        local dep_name=$(basename "$dep")
        
        if [ -f "$frameworks_dir/$dep_name" ]; then
            if [ "$dep" != "@rpath/$dep_name" ]; then
                install_name_tool -change "$dep" "@rpath/$dep_name" "$exec_path" 2>/dev/null || true
                echo "    - Repointed $dep_name to bundled version"
            fi
        elif echo "$dep" | grep -qE "^(/opt/homebrew|/usr/local/(Cellar|opt|lib))"; then
            install_name_tool -change "$dep" "@rpath/$dep_name" "$exec_path" 2>/dev/null || true
            echo "    - Repointed $dep_name (Homebrew absolute path) to @rpath"
        fi
    done
}

# --- Rewrite ALL Homebrew Absolute Paths ---
# Final sweep: scans a binary and rewrites every remaining /opt/homebrew/ or
# /usr/local/Cellar|opt|lib reference to @rpath/basename.
rewrite_homebrew_paths() {
    local binary_path="$1"
    
    if [ ! -f "$binary_path" ]; then
        return
    fi
    
    chmod +w "$binary_path" 2>/dev/null || true
    
    local deps=$(otool -L "$binary_path" 2>/dev/null | grep -v "^$binary_path:" | awk '{print $1}')
    local rewrote=0
    
    for dep in $deps; do
        if echo "$dep" | grep -qE "^(/opt/homebrew|/usr/local/(Cellar|opt|lib))"; then
            local dep_name=$(basename "$dep")
            install_name_tool -change "$dep" "@rpath/$dep_name" "$binary_path" 2>/dev/null || true
            rewrote=$((rewrote + 1))
        fi
    done
    
    if [ $rewrote -gt 0 ]; then
        echo "    - Rewrote $rewrote Homebrew path(s) in $(basename "$binary_path")"
    fi
}

# --- Logging Utilities ---
log_info() {
    echo "[INFO] $1"
}

log_error() {
    echo "[ERROR] $1" >&2
}

log_warning() {
    echo "[WARNING] $1"
}

log_step() {
    echo ""
    echo "[STEP $1] $2"
}

# --- File Utilities ---
safe_rm_rf() {
    local path="$1"
    if [ -d "$path" ] || [ -L "$path" ]; then
        rm -rf "$path"
    fi
}

ensure_dir() {
    local dir="$1"
    if [ ! -d "$dir" ]; then
        mkdir -p "$dir"
    fi
}

# --- Validation ---
check_command() {
    local cmd="$1"
    if ! command -v "$cmd" &> /dev/null; then
        return 1
    fi
    return 0
}

verify_file() {
    local file="$1"
    local description="$2"
    
    if [ ! -f "$file" ]; then
        log_error "$description not found: $file"
        return 1
    fi
    return 0
}

verify_dir() {
    local dir="$1"
    local description="$2"
    
    if [ ! -d "$dir" ]; then
        log_error "$description not found: $dir"
        return 1
    fi
    return 0
}
