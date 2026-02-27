#!/bin/bash
# =============================================================================
# AstroSpike Distribution Packager for macOS
# Creates a standalone .app bundle with all dependencies
# =============================================================================

set -e

# Check for silent mode
SILENT_MODE=0
if [ "$1" == "--silent" ]; then
    SILENT_MODE=1
fi

if [ $SILENT_MODE -eq 0 ]; then
    echo "==========================================="
    echo " AstroSpike Distribution Packager (macOS)"
    echo "==========================================="
    echo ""
fi

# Move to project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."
PROJECT_ROOT="$(pwd)"

# Load utilities
if [ -f "$SCRIPT_DIR/macos_utils.sh" ]; then
    source "$SCRIPT_DIR/macos_utils.sh"
else
    echo "[ERROR] macos_utils.sh not found!"
    exit 1
fi

BUILD_DIR="build"
DIST_DIR="dist/AstroSpike.app"
APP_BUNDLE="$BUILD_DIR/AstroSpike.app"
ERROR_COUNT=0

# --- Read version ---
VERSION=$(get_version)
if [ $SILENT_MODE -eq 0 ]; then
    echo "[INFO] Packaging version: $VERSION"
fi

# --- Verify build exists ---
echo ""
log_step 1 "Verifying build..."

verify_dir "$APP_BUNDLE" "AstroSpike.app" || {
    echo "Please run ./src/build_macos.sh first."
    exit 1
}
echo "  - AstroSpike.app: OK"

# --- Clean old dist ---
echo ""
log_step 2 "Preparing distribution folder..."

safe_rm_rf "dist"
ensure_dir "dist"

# --- Copy app bundle ---
echo ""
echo "[STEP 3] Copying app bundle..."

cp -R "$APP_BUNDLE" "$DIST_DIR"
echo "  - App bundle copied"

# --- Run macdeployqt ---
echo ""
log_step 4 "Running macdeployqt..."

QT_PREFIX=$(detect_qt_prefix)
MACDEPLOYQT=$(find_macdeployqt "$QT_PREFIX")

if [ -f "$MACDEPLOYQT" ]; then
    EXECUTABLE="$DIST_DIR/Contents/MacOS/AstroSpike"
    TARGET_ARCH=$(detect_build_architecture "$EXECUTABLE")

    LIBPATH_ARGS="-libpath=$QT_PREFIX/lib"
    
    if [ "$TARGET_ARCH" == "arm64" ]; then
        if [ -d "/opt/homebrew/lib" ]; then
            LIBPATH_ARGS="$LIBPATH_ARGS -libpath=/opt/homebrew/lib"
        fi
    else
        if [ -d "/usr/local/lib" ]; then
             LIBPATH_ARGS="$LIBPATH_ARGS -libpath=/usr/local/lib"
        fi
    fi
    
    "$MACDEPLOYQT" "$DIST_DIR" \
        -verbose=1 \
        $LIBPATH_ARGS \
        2>&1 | grep -v "Cannot resolve rpath" | grep -v "using QList" | grep -v "No such file or directory" || true
    echo "  - Qt frameworks deployed"
else
    echo "[WARNING] macdeployqt not found. Qt frameworks not bundled."
    echo "  Install Qt6 with: brew install qt@6"
    ERROR_COUNT=$((ERROR_COUNT + 1))
fi

# --- Copy Homebrew dylibs ---
echo ""
log_step 5 "Copying Homebrew libraries..."

EXECUTABLE="$DIST_DIR/Contents/MacOS/AstroSpike"
BUILD_ARCH=$(detect_build_architecture "$EXECUTABLE")
echo "  - Target architecture: $BUILD_ARCH"

FRAMEWORKS_DIR="$DIST_DIR/Contents/Frameworks"
ensure_dir "$FRAMEWORKS_DIR"

copy_dylib "libz.dylib" "zlib" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || \
copy_dylib "libz" "zlib" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true
copy_dylib "libz.1.dylib" "zlib" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true

copy_dylib "libcfitsio" "cfitsio" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true
copy_dylib "liblz4" "lz4" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true
copy_dylib "libzstd" "zstd" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true
copy_dylib "libomp" "libomp" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true
copy_dylib "libbrotlicommon" "brotli" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true
copy_dylib "libbrotlidec" "brotli" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true

copy_dylib "libopenblas.0" "openblas" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || \
copy_dylib "libopenblas" "openblas" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true

# GCC/Fortran dependencies (required by math libraries like OpenBLAS)
copy_dylib "libgcc_s.1.1" "gcc" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || \
copy_dylib "libgcc_s.1" "gcc" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true
copy_dylib "libgfortran.5" "gcc" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || \
copy_dylib "libgfortran" "gcc" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true

copy_dylib "libpng16" "libpng" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || \
copy_dylib "libpng" "libpng" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true
copy_dylib "libjpeg" "jpeg" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || \
copy_dylib "libjpeg.9" "libjpeg-turbo" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true
copy_dylib "libtiff" "libtiff" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true
copy_dylib "libtiff.6" "libtiff" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true
copy_dylib "libwebp" "libwebp" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true
copy_dylib "libwebpdemux" "libwebp" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true

copy_dylib "libfreetype" "freetype" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true
copy_dylib "libharfbuzz" "harfbuzz" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true

copy_dylib "liblapack" "lapack" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true

# Transitive dependencies that macdeployqt often misses
copy_dylib "libtbb.12" "tbb" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || \
copy_dylib "libtbb" "tbb" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true
copy_dylib "libopenjp2.7" "openjpeg" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || \
copy_dylib "libopenjp2" "openjpeg" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true

# OpenCV
OPENCV_PREFIX=$(brew --prefix opencv 2>/dev/null || echo "")
if [ ! -d "$OPENCV_PREFIX/lib" ]; then
    for base in /opt/homebrew /usr/local; do
        if [ -d "$base/Cellar/opencv" ]; then
            OPENCV_PREFIX=$(find "$base/Cellar/opencv" -maxdepth 2 -name "lib" -type d 2>/dev/null | head -1)
            if [ -n "$OPENCV_PREFIX" ]; then 
                OPENCV_PREFIX=$(dirname "$OPENCV_PREFIX")
                break
            fi
        fi
    done
fi

if [ -n "$OPENCV_PREFIX" ] && [ -d "$OPENCV_PREFIX/lib" ]; then
    OPENCV_MODULES="core imgproc imgcodecs photo"
    
    COPIED_COUNT=0
    for module in $OPENCV_MODULES; do
        for dylib in "$OPENCV_PREFIX/lib"/libopencv_${module}*.dylib; do
            if [ -f "$dylib" ]; then
                cp "$dylib" "$FRAMEWORKS_DIR/" 2>/dev/null || true
                COPIED_COUNT=$((COPIED_COUNT + 1))
            fi
        done
    done
    
    if [ $COPIED_COUNT -gt 0 ]; then
        echo "  - OpenCV (core, imgproc, imgcodecs, photo): OK"
    else
        echo "  - OpenCV: NOT FOUND"
    fi
    
    PROBLEMATIC_LIBS=$(find "$FRAMEWORKS_DIR" -name "*openvino*" -o -name "*protobuf*" 2>/dev/null | grep -v "/Applications" || true)
    if [ -n "$PROBLEMATIC_LIBS" ]; then
        echo "  [WARNING] Found external dependencies that should not be bundled:"
        echo "$PROBLEMATIC_LIBS" | xargs rm -f
        echo "    Removed problematic dylibs"
    fi
else
    echo "  - OpenCV: NOT FOUND"
fi

# --- Copy resources ---
echo ""
log_step 6 "Copying resources..."

RESOURCES_DIR="$DIST_DIR/Contents/Resources"

if [ -d "src/images" ]; then
    ensure_dir "$RESOURCES_DIR/images"
    cp -R src/images/* "$RESOURCES_DIR/images/"
    echo "  - Images: OK"
fi

# --- Resolve & Fix Libraries ---
echo ""
log_step 7 "Resolving and Fixing Libraries..."

EXECUTABLE="$DIST_DIR/Contents/MacOS/AstroSpike"

# 1. Recursive copy of missing dependencies
echo "  - Recursively collecting dependencies..."
for i in {1..3}; do
    for dylib in "$FRAMEWORKS_DIR"/*.dylib; do
        if [ -f "$dylib" ]; then
            copy_dylib_with_dependencies "$dylib" "$FRAMEWORKS_DIR" "$BUILD_ARCH" || true
        fi
    done
done

# 2. Fix dylib IDs and internal dependencies
echo "  - Fixing dylib IDs and paths..."
for dylib in "$FRAMEWORKS_DIR"/*.dylib; do
    if [ -f "$dylib" ]; then
        fix_dylib_id_and_deps "$dylib" "$FRAMEWORKS_DIR"
    fi
done

# 3. Fix executable dependencies
if [ -f "$EXECUTABLE" ]; then
    echo "  - Fixing executable dependencies..."
    install_name_tool -add_rpath "@executable_path/../Frameworks" "$EXECUTABLE" 2>/dev/null || true
    fix_executable_deps "$EXECUTABLE" "$FRAMEWORKS_DIR"
fi

# 4. Final sweep: rewrite ALL remaining Homebrew absolute paths
echo "  - Final sweep: rewriting any remaining Homebrew paths..."
rewrite_homebrew_paths "$EXECUTABLE"
for dylib in "$FRAMEWORKS_DIR"/*.dylib; do
    if [ -f "$dylib" ]; then
        rewrite_homebrew_paths "$dylib"
    fi
done

# 5. Fix absolute paths inside Qt Framework binaries
echo "  - Fixing dependencies inside .framework bundles..."
for framework in "$FRAMEWORKS_DIR"/*.framework; do
    if [ -d "$framework" ]; then
        framework_name=$(basename "$framework" .framework)
        framework_binary="$framework/Versions/A/$framework_name"
        
        if [ -f "$framework_binary" ]; then
            rewrite_homebrew_paths "$framework_binary"
        fi
    fi
done

# 6. Fix absolute paths inside Qt Plugins
echo "  - Fixing dependencies inside Qt Plugins..."
PLUGINS_DIR="$DIST_DIR/Contents/PlugIns"
if [ -d "$PLUGINS_DIR" ]; then
    find "$PLUGINS_DIR" -name "*.dylib" | while read -r plugin_path; do
        if [ -f "$plugin_path" ]; then
            rewrite_homebrew_paths "$plugin_path"
            install_name_tool -add_rpath "@executable_path/../Frameworks" "$plugin_path" 2>/dev/null || true
        fi
    done
fi

# --- Verify bundled dylibs dependencies ---
echo ""
echo "[STEP 7.1] Verifying bundled dependencies..."

MISSING_DEPS=0
HOMEBREW_REFS=0

echo "  - Checking for unresolved @rpath references..."
for dylib in "$FRAMEWORKS_DIR"/*.dylib; do
    if [ -f "$dylib" ]; then
        UNRESOLVED=$(otool -L "$dylib" 2>/dev/null | grep "@rpath" | grep -v "^$dylib:" | grep -v "@rpath/Qt" || true)
        if [ -n "$UNRESOLVED" ]; then
            while IFS= read -r dep_line; do
                DEP_NAME=$(echo "$dep_line" | awk '{print $1}' | sed 's|@rpath/||')
                if [ -n "$DEP_NAME" ] && [ "$DEP_NAME" != "@rpath" ]; then
                    if [ ! -f "$FRAMEWORKS_DIR/$DEP_NAME" ]; then
                        echo "  [WARNING] Unresolved @rpath: $(basename "$dylib") -> $DEP_NAME"
                        MISSING_DEPS=$((MISSING_DEPS + 1))
                    fi
                fi
            done <<< "$UNRESOLVED"
        fi
        
        BREW_REFS=$(otool -L "$dylib" 2>/dev/null | grep -v "^$dylib:" | awk '{print $1}' | grep -E "^(/opt/homebrew|/usr/local/(Cellar|opt|lib))" || true)
        if [ -n "$BREW_REFS" ]; then
            while IFS= read -r brew_ref; do
                echo "  [WARNING] Absolute Homebrew path in $(basename "$dylib"): $brew_ref"
                HOMEBREW_REFS=$((HOMEBREW_REFS + 1))
            done <<< "$BREW_REFS"
        fi
    fi
done

if [ -f "$EXECUTABLE" ]; then
    EXEC_BREW_REFS=$(otool -L "$EXECUTABLE" 2>/dev/null | grep -v "^$EXECUTABLE:" | awk '{print $1}' | grep -E "^(/opt/homebrew|/usr/local/(Cellar|opt|lib))" || true)
    if [ -n "$EXEC_BREW_REFS" ]; then
        while IFS= read -r brew_ref; do
            echo "  [WARNING] Absolute Homebrew path in AstroSpike executable: $brew_ref"
            HOMEBREW_REFS=$((HOMEBREW_REFS + 1))
        done <<< "$EXEC_BREW_REFS"
    fi
fi

if [ $MISSING_DEPS -gt 0 ]; then
    echo "  [WARNING] Found $MISSING_DEPS unresolved @rpath dependencies"
else
    echo "  - All @rpath dylib dependencies resolved"
fi

if [ $HOMEBREW_REFS -gt 0 ]; then
    echo "  [WARNING] Found $HOMEBREW_REFS absolute Homebrew path(s) remaining!"
else
    echo "  - No absolute Homebrew paths detected (portable)"
fi

# --- Verify critical libraries ---
echo ""
echo "[STEP 7.2] Verifying critical libraries..."

ZLIB_FOUND=0
for zlib_name in libz.dylib libz.1.dylib libz libz.1; do
    if [ -f "$FRAMEWORKS_DIR/$zlib_name" ]; then
        ZLIB_FOUND=1
        break
    fi
done

if [ $ZLIB_FOUND -eq 0 ]; then
    echo "  [CRITICAL ERROR] ZLIB NOT FOUND in bundle!"
    ERROR_COUNT=$((ERROR_COUNT + 1))
else
    echo "  - ZLIB: OK"
fi

for imglib in libpng libjpeg libtiff libwebp; do
    if find "$FRAMEWORKS_DIR" -name "$imglib*" 2>/dev/null | head -1 | grep -q .; then
        echo "  - $imglib: OK"
    else
        echo "  [WARNING] $imglib NOT FOUND - some image formats may fail"
    fi
done

# --- Ad-hoc Code Signing ---
echo ""
log_step 8 "Applying ad-hoc code signing..."

check_command codesign && {
    codesign --force --deep -s - "$DIST_DIR"
    echo "  - Ad-hoc signed: OK"
} || {
    log_warning "codesign not found (skip)"
}

# --- Create README ---
echo ""
echo "[STEP 9] Creating README..."

cat > "dist/README.txt" << EOF
AstroSpike v$VERSION - Artificial Star Spike Generator
============================================================

INSTALLATION:
Drag AstroSpike.app to your Applications folder.

FIRST RUN:
Right-click AstroSpike.app and select "Open" to bypass Gatekeeper
on first launch (since the app is not notarized).

GitHub: https://github.com/Ft2801/AstroSpike
EOF

echo "  - README.txt: OK"

cp "changelog.txt" "dist/" 2>/dev/null || true

# --- Summary ---
echo ""
echo "==========================================="
if [ $ERROR_COUNT -eq 0 ]; then
    echo " SUCCESS! Distribution ready"
else
    echo " COMPLETED WITH $ERROR_COUNT WARNING(S)"
fi
echo " Location: dist/AstroSpike.app"
echo "==========================================="
echo ""
echo "Next step: ./src/build_installer_macos.sh"
