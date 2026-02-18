#include "ImageBuffer.h"
#include "core/SimdOps.h"
#include "io/SimpleTiffWriter.h"
#include "io/SimpleTiffReader.h"
#include <QtConcurrent/QtConcurrent>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <omp.h>
#include <QDebug>
#include "core/RobustStatistics.h"
#include <QBuffer>
#include <QPainter>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDataStream> 
#include <stack>
#include <cmath> 
#include "io/FitsLoader.h"
#include <opencv2/opencv.hpp>

ImageBuffer::ImageBuffer() : m_mutex(std::make_unique<QReadWriteLock>(QReadWriteLock::Recursive)) {}

ImageBuffer::ImageBuffer(int width, int height, int channels) 
    : m_width(width), m_height(height), m_channels(channels),
      m_mutex(std::make_unique<QReadWriteLock>(QReadWriteLock::Recursive))
{
    m_data.resize(static_cast<size_t>(width) * height * channels, 0.0f);
}

ImageBuffer::ImageBuffer(const ImageBuffer& other)
    : m_width(other.m_width), m_height(other.m_height), m_channels(other.m_channels),
      m_data(other.m_data), m_meta(other.m_meta), m_name(other.m_name),
      m_modified(other.m_modified)
{
    m_mutex = std::make_unique<QReadWriteLock>(QReadWriteLock::Recursive);
}

ImageBuffer& ImageBuffer::operator=(const ImageBuffer& other) {
    if (this != &other) {
        m_width = other.m_width;
        m_height = other.m_height;
        m_channels = other.m_channels;
        m_data = other.m_data;
        m_meta = other.m_meta;
        m_name = other.m_name;
        m_modified = other.m_modified;
        if (!m_mutex) m_mutex = std::make_unique<QReadWriteLock>(QReadWriteLock::Recursive);
    }
    return *this;
}

ImageBuffer::~ImageBuffer() {
}


const std::vector<float>& ImageBuffer::data() const {
    Q_ASSERT(!m_data.empty() || (m_width*m_height*m_channels == 0));
    return m_data;
}

std::vector<float>& ImageBuffer::data() {
    Q_ASSERT(!m_data.empty() || (m_width*m_height*m_channels == 0));
    return m_data;
}


void ImageBuffer::setData(int width, int height, int channels, const std::vector<float>& data) {
    m_width = width;
    m_height = height;
    m_channels = channels;
    m_data = data;
    if (m_data.empty() && width > 0 && height > 0 && channels > 0) {
        m_data.resize(static_cast<size_t>(width) * height * channels, 0.0f);
    }
}

void ImageBuffer::resize(int width, int height, int channels) {
    m_width = width;
    m_height = height;
    m_channels = channels;
    m_data.assign(static_cast<size_t>(width) * height * channels, 0.0f);
}

bool ImageBuffer::loadRegion(const QString& filePath, int x, int y, int w, int h, QString* errorMsg) {
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    
    if (ext == "fit" || ext == "fits") {
        return FitsLoader::loadRegion(filePath, *this, x, y, w, h, errorMsg);
    }
    
    bool loaded = false;
    if (ext == "tif" || ext == "tiff") {
        loaded = loadTiff32(filePath, errorMsg);
    } else {
        loaded = loadStandard(filePath);
    }
    
    if (loaded) {
        crop(x, y, w, h);
        return true;
    }
    
    if (errorMsg && errorMsg->isEmpty()) {
        *errorMsg = "Failed to open file or format not supported";
    }
    return false;
}

bool ImageBuffer::loadStandard(const QString& filePath) {
    // Use OpenCV for standard formats (JPG, PNG, etc.)
    std::string stdPath = filePath.toStdString();
    cv::Mat img = cv::imread(stdPath, cv::IMREAD_UNCHANGED);
    
    if (img.empty()) return false;

    int w = img.cols;
    int h = img.rows;
    int ch = img.channels();
    
    // Normalize channel count to 3 (RGB)
    if (ch == 1) {
        cv::cvtColor(img, img, cv::COLOR_GRAY2BGR);
        ch = 3;
    } else if (ch == 4) {
        cv::cvtColor(img, img, cv::COLOR_BGRA2BGR);
        ch = 3;
    } else if (ch != 3) {
        // Fallback for weird channel counts
        return false;
    }
    
    // Convert to float32 and normalize to [0,1]
    cv::Mat floatMat;
    double scale = 1.0;
    
    switch (img.depth()) {
        case CV_8U:  scale = 1.0 / 255.0; break;
        case CV_16U: scale = 1.0 / 65535.0; break;
        case CV_32F: scale = 1.0; break;
        default:     scale = 1.0 / 255.0; break; // Assume 8-bit for others
    }
    
    img.convertTo(floatMat, CV_32FC3, scale);
    
    m_width = w;
    m_height = h;
    m_channels = 3;
    m_data.resize(static_cast<size_t>(w) * h * 3);

    // Copy and BGR -> RGB
    for (int y = 0; y < h; ++y) {
        const float* row = floatMat.ptr<float>(y);
        for (int x = 0; x < w; ++x) {
            size_t dstIdx = (static_cast<size_t>(y) * w + x) * 3;
            size_t srcIdx = x * 3;
            m_data[dstIdx + 0] = row[srcIdx + 2]; // R
            m_data[dstIdx + 1] = row[srcIdx + 1]; // G
            m_data[dstIdx + 2] = row[srcIdx + 0]; // B
        }
    }
    
    return true;
}

bool ImageBuffer::loadTiff32(const QString& filePath, QString* errorMsg, QString* debugInfo) {
    // Use OpenCV for fast and reliable TIFF loading (uses libtiff internally)
    std::string stdPath = filePath.toStdString();
    
    // IMREAD_UNCHANGED preserves bit depth and channel count
    cv::Mat img = cv::imread(stdPath, cv::IMREAD_UNCHANGED);
    
    if (img.empty()) {
        // Fallback to SimpleTiffReader (handles 32-bit unsigned properly)
        int w, h, c;
        std::vector<float> data;
        if (SimpleTiffReader::readFloat32(filePath, w, h, c, data, errorMsg, debugInfo)) {
            setData(w, h, c, data);
            return true;
        }
        return false;
    }
    
    int w = img.cols;
    int h = img.rows;
    int ch = img.channels();
    
    // Force to 3 channels if grayscale (for ImageBuffer compatibility)
    if (ch == 1) {
        cv::cvtColor(img, img, cv::COLOR_GRAY2BGR);
        ch = 3;
    } else if (ch == 4) {
        // Drop alpha channel
        cv::cvtColor(img, img, cv::COLOR_BGRA2BGR);
        ch = 3;
    }
    
    // Convert to float32 and normalize to [0,1]
    cv::Mat floatMat;
    double scale = 1.0;
    
    switch (img.depth()) {
        case CV_8U:  scale = 1.0 / 255.0; break;
        case CV_16U: scale = 1.0 / 65535.0; break;
        case CV_32S: 
            // 32-bit signed - but TIFF might actually be unsigned, try SimpleTiffReader
            {
                int tw, th, tc;
                std::vector<float> tdata;
                if (SimpleTiffReader::readFloat32(filePath, tw, th, tc, tdata, errorMsg, debugInfo)) {
                    setData(tw, th, tc, tdata);
                    return true;
                }
            }
            scale = 1.0 / 2147483647.0; 
            break;
        case CV_32F: scale = 1.0; break; // Already float
        case CV_64F: scale = 1.0; break; // Will be converted
        default:
            if (errorMsg) *errorMsg = QObject::tr("Unsupported TIFF bit depth.");
            return false;
    }
    
    img.convertTo(floatMat, CV_32FC(ch), scale);
    
    // Copy to our data structure (BGR -> RGB and row-major interleaved)
    std::vector<float> data(w * h * ch);
    
    for (int y = 0; y < h; ++y) {
        const float* row = floatMat.ptr<float>(y);
        for (int x = 0; x < w; ++x) {
            int srcIdx = x * ch;
            int dstIdx = (y * w + x) * ch;
            // BGR to RGB swap
            data[dstIdx + 0] = row[srcIdx + 2]; // R
            data[dstIdx + 1] = row[srcIdx + 1]; // G
            data[dstIdx + 2] = row[srcIdx + 0]; // B
        }
    }
    
    setData(w, h, ch, data);
    
    if (debugInfo) {
        *debugInfo = QString("Loaded via OpenCV: %1x%2, %3ch, depth=%4").arg(w).arg(h).arg(ch).arg(img.depth());
    }
    
    return true;
}


// ------ Advanced Display Logic ------

// Constants for LUT
static const int LUT_SIZE = 65536;

// Statistics Helper
struct ChStats { float median; float mad; };

// MTF helper: y = (m-1)x / ((2m-1)x - m)
static float mtf_func(float m, float x) {
    if (x <= 0) return 0;
    if (x >= 1) return 1;
    if (m <= 0) return 0;
    if (m >= 1) return x;
    return ((m - 1.0f) * x) / ((2.0f * m - 1.0f) * x - m);
}

#include <QSettings>

// High Precision (24-bit/Float) Stats (No Histogram Binning)
static ChStats computeStatsHighPrecision(const std::vector<float>& data, int width, int height, int channels, int channelIndex) {
    const float MAD_NORM = 1.4826f;
    
    long totalPixels = static_cast<long>(width) * height;
    if (totalPixels == 0) return {0.0f, 0.0f};
    
    // Subsampling strategy (Same as Histogram to match performance tier, but using exact float values)
    int step = 1;
    if (totalPixels > 4000000) { // > 4MP
        step = static_cast<int>(std::sqrt(static_cast<double>(totalPixels) / 4000000.0));
        if (step < 1) step = 1;
    }

    // Collect samples
    // Estimate size for reservation
    size_t estSize = (totalPixels / (step * step)) + 1000;
    std::vector<float> samples;
    samples.reserve(estSize);

    for (int y = 0; y < height; y += step) {
        for (int x = 0; x < width; x += step) {
             size_t idx = (static_cast<size_t>(y) * width + x) * channels + channelIndex;
             if (idx < data.size()) {
                 float v = data[idx];
                 if (v >= 0.0f && v <= 1.0f) {
                     samples.push_back(v);
                 }
             }
        }
    }
    
    if (samples.empty()) return {0.0f, 0.0f};

    // 1. Find Median
    size_t n = samples.size();
    size_t mid = n / 2;
    std::nth_element(samples.begin(), samples.begin() + mid, samples.end());
    float median = samples[mid];

    // 2. Find MAD
    // Reuse samples vector for deviations to save memory
    for (size_t i = 0; i < n; ++i) {
        samples[i] = std::fabs(samples[i] - median);
    }
    
    std::nth_element(samples.begin(), samples.begin() + mid, samples.end());
    float mad = samples[mid] * MAD_NORM;

    return {median, mad};
}

static ChStats computeStats(const std::vector<float>& data, int width, int height, int channels, int channelIndex) {
    // Check Settings for 24-bit Override
    QSettings settings;
    if (settings.value("display/24bit_stf", true).toBool()) {
        return computeStatsHighPrecision(data, width, height, channels, channelIndex);
    }
    
    const int HIST_SIZE = 65536;
    const float MAD_NORM = 1.4826f; // Standard Normalization Factor for MAD
    std::vector<int> hist(HIST_SIZE, 0);
    
    long totalPixels = static_cast<long>(width) * height;
    if (totalPixels == 0) return {0.0f, 0.0f};
    
    // Subsampling strategy
    int step = 1;
    if (totalPixels > 4000000) { // > 4MP
        step = static_cast<int>(std::sqrt(static_cast<double>(totalPixels) / 4000000.0));
        if (step < 1) step = 1;
    }

    long count = 0;
    
    // 1. Build Histogram
    for (int y = 0; y < height; y += step) {
        for (int x = 0; x < width; x += step) {
             size_t idx = (static_cast<size_t>(y) * width + x) * channels + channelIndex;
             if (idx < data.size()) {
                 float v = data[idx];
                 v = std::max(0.0f, std::min(1.0f, v));
                 int iVal = static_cast<int>(v * (HIST_SIZE - 1) + 0.5f);
                 hist[iVal]++;
                 count++;
             }
        }
    }
    
    if (count == 0) return {0.0f, 0.0f};

    // 2. Find Median
    long medianIdx = -1;
    long currentSum = 0;
    long medianLevel = count / 2;
    
    for (int i = 0; i < HIST_SIZE; ++i) {
        currentSum += hist[i];
        if (currentSum >= medianLevel) {
            medianIdx = i;
            break;
        }
    }
    
    float median = (float)medianIdx / (HIST_SIZE - 1);
    
    // 3. Find MAD (Median Absolute Deviation)
    std::vector<int> madHist(HIST_SIZE, 0);
    for (int i = 0; i < HIST_SIZE; ++i) {
        if (hist[i] > 0) {
            int dev = std::abs(i - (int)medianIdx);
            madHist[dev] += hist[i];
        }
    }
    
    // Find Median of MAD Hist
    currentSum = 0;
    long madIdx = -1;
    for (int i = 0; i < HIST_SIZE; ++i) {
        currentSum += madHist[i];
        if (currentSum >= medianLevel) {
            madIdx = i;
            break;
        }
    }
    
    float rawMad = (float)madIdx / (HIST_SIZE - 1);
    float mad = rawMad * MAD_NORM; // Apply normalization for Gaussian consistency
    
    return {median, mad};
}

// ====== Standard MTF Function ======
// Added safety guards for edge cases
[[maybe_unused]] static float standardMTF(float x, float m, float lo, float hi) {
    if (x <= lo) return 0.f;
    if (x >= hi) return 1.f;
    if (hi <= lo) return 0.5f; // Safety: avoid division by zero
    
    float xp = (x - lo) / (hi - lo);
    
    // Safety: handle m = 0.5 case (causes 2m-1 = 0)
    float denom = ((2.f * m - 1.f) * xp) - m;
    if (std::fabs(denom) < 1e-9f) return 0.5f;
    
    float result = ((m - 1.f) * xp) / denom;
    
    // Safety: clamp result to valid range
    if (std::isnan(result) || std::isinf(result)) return 0.5f;
    return std::clamp(result, 0.f, 1.f);
}

// Standard mtf_params equivalent
struct StandardSTFParams {
    float shadows;   // lo (black point)
    float midtones;  // m (midtone balance, 0-1)
    float highlights; // hi (white point)
};

// Computes standard AutoStretch params for a single channel
[[maybe_unused]] static StandardSTFParams computeStandardSTF(const std::vector<float>& data, [[maybe_unused]] int w, [[maybe_unused]] int h, int ch, int channelIdx) {
    const float AS_DEFAULT_SHADOWS_CLIPPING = -2.80f;
    const float AS_DEFAULT_TARGET_BACKGROUND = 0.25f;
    
    StandardSTFParams result;
    result.highlights = 1.0f;
    result.shadows = 0.0f;
    result.midtones = 0.25f; // Default fallback (neutral)
    
    if (data.empty() || ch <= 0) return result;
    
    // Extract channel data
    std::vector<float> chData;
    chData.reserve(data.size() / ch);
    for (size_t i = channelIdx; i < data.size(); i += ch) {
        chData.push_back(data[i]);
    }
    if (chData.size() < 2) return result;  // Need at least 2 samples for meaningful stats
    
    // Compute median and MAD
    std::vector<float> sorted = chData;
    std::sort(sorted.begin(), sorted.end());
    float median = sorted[sorted.size() / 2];
    
    // Safe Median Logic
    if (median < 1e-6f) {
        // Fallback to Mean if Median is zero (common in star masks)
        double sum = 0;
        for(float v : chData) sum += v;
        median = (float)(sum / chData.size());
        
        // If still zero, force a small epsilon
        if (median < 1e-6f) median = 0.0001f;
    }
    
    std::vector<float> deviations(sorted.size());
    for (size_t i = 0; i < sorted.size(); ++i) {
        deviations[i] = std::fabs(sorted[i] - median);
    }
    std::sort(deviations.begin(), deviations.end());
    float mad = deviations[deviations.size() / 2];
    
    // Guard against MAD = 0 (Standard does this)
    if (mad < 1e-9f) mad = 0.001f;
    
    // Standard MAD_NORM = 1.4826
    float sigma = 1.4826f * mad;
    
    // shadows = median + clipping * sigma (clipping is negative, so this subtracts)
    float c0 = median + AS_DEFAULT_SHADOWS_CLIPPING * sigma;
    if (c0 < 0.f) c0 = 0.f;
    
    // m2 = median - c0 (the "distance" from shadow to median)
    float m2 = median - c0;
    
    result.shadows = c0;
    result.highlights = 1.0f;
    
    // Standard formula for midtones: MTF(m2, target_bg, 0, 1)
    float target = AS_DEFAULT_TARGET_BACKGROUND;
    if (m2 <= 1e-9f || m2 >= 1.f) {
        // If m2 is super small, it means shadow ~ median. 
        // We need extreme stretch. 
        result.midtones = 0.001f; // Force strong stretch
    } else {
        float xp = m2;
        float denom = ((2.f * target - 1.f) * xp) - target;
        if (std::fabs(denom) < 1e-9f) {
            result.midtones = 0.25f;
        } else {
            result.midtones = ((target - 1.f) * xp) / denom;
        }
        // Clamp to valid range
        if (std::isnan(result.midtones) || std::isinf(result.midtones)) {
            result.midtones = 0.25f;
        } else {
            result.midtones = std::clamp(result.midtones, 0.00001f, 0.99999f); // Allow stronger stretch than 0.001
        }
    }
    
    return result;
}

#include <fitsio.h>

// ------ Saving Logic ------
bool ImageBuffer::save(const QString& filePath, const QString& format, BitDepth depth, QString* errorMsg) const {
    if (m_data.empty()) return false;

    if (format.compare("xisf", Qt::CaseInsensitive) == 0) {
        if (errorMsg) *errorMsg = "XISF saving is not supported.";
        return false;
    }

    // FITS uses CFITSIO
    if (format.compare("fits", Qt::CaseInsensitive) == 0 || format.compare("fit", Qt::CaseInsensitive) == 0) {
        fitsfile* fptr;
        int status = 0;
        
        // Overwrite by prefixing "!" (CFITSIO magic)
        QString outName = "!" + filePath;
        
        if (fits_create_file(&fptr, outName.toUtf8().constData(), &status)) {
            if (errorMsg) *errorMsg = "CFITSIO Create File Error: " + QString::number(status);
            return false;
        }

        // Determine BITPIX
        int bitpix = FLOAT_IMG; // Default -32
        if (depth == Depth_32Int) bitpix = LONG_IMG; // 32
        else if (depth == Depth_16Int) bitpix = SHORT_IMG; // 16
        else if (depth == Depth_8Int) bitpix = BYTE_IMG; // 8
        
        long naxes[3] = { (long)m_width, (long)m_height, (long)m_channels };
        int naxis = (m_channels > 1) ? 3 : 2;

        if (fits_create_img(fptr, bitpix, naxis, naxes, &status)) {
            if (errorMsg) *errorMsg = "CFITSIO Create Image Error: " + QString::number(status);
            fits_close_file(fptr, &status);
            return false;
        }

        // Prepare data to write
        long nelements = m_width * m_height * m_channels;
        
        if (depth == Depth_32Float) {
             
             std::vector<float> planarData(nelements);
             if (m_channels == 1) {
                 planarData = m_data; // Copy
             } else {
                 long planeSize = m_width * m_height;
                 for (int i = 0; i < planeSize; ++i) {
                     planarData[i] = m_data[i*3 + 0];             // R (Plane 1)
                     planarData[i + planeSize] = m_data[i*3 + 1]; // G (Plane 2)
                     planarData[i + 2*planeSize] = m_data[i*3 + 2];// B (Plane 3)
                 }
             }

             if (fits_write_img(fptr, TFLOAT, 1, nelements, planarData.data(), &status)) {
                 if (errorMsg) *errorMsg = "CFITSIO Write Error: " + QString::number(status);
                 fits_close_file(fptr, &status);
                 return false;
             }

        } else {
            // Integer conversion
            double bscale = 1.0;
            double bzero = 0.0;
            
            if (depth == Depth_16Int) {
                // UInt16: [0, 65535] -> [-32768, 32767]
                bzero = 32768.0;
                bscale = 1.0;
                fits_write_key(fptr, TDOUBLE, "BZERO", &bzero, "offset for unsigned integers", &status);
                fits_write_key(fptr, TDOUBLE, "BSCALE", &bscale, "scaling", &status);
            } else if (depth == Depth_32Int) {
                // UInt32: [0, 4294967295] -> [-2147483648, 2147483647]
                // BITPIX=32 is signed 32-bit.
                // We use standard BZERO = 2147483648 to represent unsigned 32-bit.
                bzero = 2147483648.0;
                bscale = 1.0;
                fits_write_key(fptr, TDOUBLE, "BZERO", &bzero, "offset for unsigned integers", &status);
                fits_write_key(fptr, TDOUBLE, "BSCALE", &bscale, "scaling", &status);
            } else {
                // 8-bit is unsigned natively (BYTE_IMG)
            }

            // Convert and Deshuffle
            std::vector<float> planarData(nelements);
            // MaxVal depends on target range
            // UInt32 max = 4294967295.0
            float maxVal = (depth == Depth_16Int) ? 65535.0f : ((depth == Depth_32Int) ? 4294967295.0f : 255.0f);
            
             if (m_channels == 1) {
                 for(int i=0; i<(int)nelements; ++i) planarData[i] = m_data[i] * maxVal;
             } else {
                 long planeSize = m_width * m_height;
                 for (int i = 0; i < planeSize; ++i) {
                     planarData[i] = m_data[i*3 + 0] * maxVal;
                     planarData[i + planeSize] = m_data[i*3 + 1] * maxVal;
                     planarData[i + 2*planeSize] = m_data[i*3 + 2] * maxVal;
                 }
             }
             
             int type = TFLOAT;
             // fits_write_img will convert TFLOAT using BZERO/BSCALE
             
             if (fits_write_img(fptr, type, 1, nelements, planarData.data(), &status)) {
                 if (errorMsg) *errorMsg = "CFITSIO Write Error: " + QString::number(status);
                 fits_close_file(fptr, &status);
                 return false;
             }
        }
        
        // --- WRITE METADATA ---
        for (const auto& card : m_meta.rawHeaders) {
            // Skip structural keywords that CFITSIO already wrote or will write
            QString key = card.key.trimmed().toUpper();
            if (key == "SIMPLE" || key == "BITPIX" || key == "NAXIS" || key == "NAXIS1" || 
                key == "NAXIS2" || key == "NAXIS3" || key == "EXTEND" || key == "BZERO" || key == "BSCALE") {
                continue;
            }
            
            if (key == "HISTORY") {
                fits_write_history(fptr, card.value.toUtf8().constData(), &status);
            } else if (key == "COMMENT") {
                fits_write_comment(fptr, card.value.toUtf8().constData(), &status);
            } else {
                // Heuristic to determine type
                bool isLong;
                long lVal = card.value.toLong(&isLong);
                
                bool isDouble;
                double dVal = card.value.toDouble(&isDouble);
                
                if (isLong) {
                     fits_write_key(fptr, TLONG, key.toUtf8().constData(), &lVal, card.comment.toUtf8().constData(), &status);
                } else if (isDouble) {
                     fits_write_key(fptr, TDOUBLE, key.toUtf8().constData(), &dVal, card.comment.toUtf8().constData(), &status);
                } else {
                     fits_write_key(fptr, TSTRING, key.toUtf8().constData(), 
                                    (void*)card.value.toUtf8().constData(), 
                                    card.comment.toUtf8().constData(), &status);
                }
            }
            
            if (status) status = 0; // Ignore error and continue
        }
        
        // Write WCS explicitly to ensure it overrides any old/invalid WCS in rawHeaders
        if (m_meta.ra != 0 || m_meta.dec != 0) {
            // CTYPE keywords are required for WCS validity
            const char* ctype1 = "RA---TAN";
            const char* ctype2 = "DEC--TAN";
            fits_update_key(fptr, TSTRING, "CTYPE1", (void*)ctype1, "Coordinate type", &status);
            fits_update_key(fptr, TSTRING, "CTYPE2", (void*)ctype2, "Coordinate type", &status);
            
            double equinox = 2000.0;
            fits_update_key(fptr, TDOUBLE, "EQUINOX", &equinox, "Equinox of coordinates", &status);
            
            fits_update_key(fptr, TDOUBLE, "CRVAL1", (void*)&m_meta.ra, "RA at reference pixel", &status);
            fits_update_key(fptr, TDOUBLE, "CRVAL2", (void*)&m_meta.dec, "Dec at reference pixel", &status);
            fits_update_key(fptr, TDOUBLE, "CRPIX1", (void*)&m_meta.crpix1, "Reference pixel x", &status);
            fits_update_key(fptr, TDOUBLE, "CRPIX2", (void*)&m_meta.crpix2, "Reference pixel y", &status);
            fits_update_key(fptr, TDOUBLE, "CD1_1", (void*)&m_meta.cd1_1, "", &status);
            fits_update_key(fptr, TDOUBLE, "CD1_2", (void*)&m_meta.cd1_2, "", &status);
            fits_update_key(fptr, TDOUBLE, "CD2_1", (void*)&m_meta.cd2_1, "", &status);
            fits_update_key(fptr, TDOUBLE, "CD2_2", (void*)&m_meta.cd2_2, "", &status);
            status = 0;
        }

        fits_close_file(fptr, &status);
        return true;

    } else if (format.compare("tiff", Qt::CaseInsensitive) == 0 || format.compare("tif", Qt::CaseInsensitive) == 0) {
        SimpleTiffWriter::Format fmt = SimpleTiffWriter::Format_uint8;
        if (depth == Depth_16Int) fmt = SimpleTiffWriter::Format_uint16;
        else if (depth == Depth_32Int) fmt = SimpleTiffWriter::Format_uint32;
        else if (depth == Depth_32Float) fmt = SimpleTiffWriter::Format_float32;
        
        if (!SimpleTiffWriter::write(filePath, m_width, m_height, m_channels, fmt, m_data, errorMsg)) {
             return false;
        }
        return true;
        
    } else {
        // Standard (JPG/PNG) via QImage
        // Convert to 8-bit RGB
        QImage saveImg = getDisplayImage(Display_Linear); 
        return saveImg.save(filePath, format.toLatin1().constData());
    }
}

QImage ImageBuffer::getDisplayImage(DisplayMode mode, bool linked, const std::vector<std::vector<float>>* overrideLUT, int maxWidth, int maxHeight, bool inverted, bool falseColor) const {
    ReadLock lock(this);
    if (m_data.empty()) return QImage();

    int w = m_width;
    int h = m_height;
    
    // Scale down if needed for display performance
    int outW = w;
    int outH = h;
    if (maxWidth > 0 && maxHeight > 0) {
        if (w > maxWidth || h > maxHeight) {
            float ratio = std::min((float)maxWidth / w, (float)maxHeight / h);
            outW = (int)(w * ratio);
            outH = (int)(h * ratio);
            if (outW < 1) outW = 1;
            if (outH < 1) outH = 1;
        }
    }
    
    QImage img(outW, outH, QImage::Format_RGB32);
    
    // Prepare LUTs
    std::vector<std::vector<uint8_t>> luts(3, std::vector<uint8_t>(65536));
    
    if (overrideLUT && overrideLUT->size() == 3 && (*overrideLUT)[0].size() == 65536) {
        // Use override
        for (int c = 0; c < 3; ++c) {
            for (int i = 0; i < 65536; ++i) {
                float v = (*overrideLUT)[c][i];
                if (inverted) v = 1.0f - v;
                luts[c][i] = static_cast<uint8_t>(std::clamp(v * 255.0f, 0.0f, 255.0f));
            }
        }
    } else if (mode == Display_AutoStretch) {
        // Auto Stretch
        std::vector<StandardSTFParams> stf(3);
        if (linked) {
            // Compute params for each channel
            stf[0] = computeStandardSTF(m_data, w, h, m_channels, 0); 
            stf[1] = computeStandardSTF(m_data, w, h, m_channels, 1); 
            stf[2] = computeStandardSTF(m_data, w, h, m_channels, 2); 
            
            // Average them
            float avgShadows = (stf[0].shadows + stf[1].shadows + stf[2].shadows) / 3.0f;
            float avgMidtones = (stf[0].midtones + stf[1].midtones + stf[2].midtones) / 3.0f;
            float avgHighlights = (stf[0].highlights + stf[1].highlights + stf[2].highlights) / 3.0f;
            
            for(int c=0; c<3; ++c) {
                stf[c] = {avgShadows, avgMidtones, avgHighlights};
            }
        } else {
             stf[0] = computeStandardSTF(m_data, w, h, m_channels, 0);
             stf[1] = computeStandardSTF(m_data, w, h, m_channels, 1);
             stf[2] = computeStandardSTF(m_data, w, h, m_channels, 2);
        }
        
        for (int c = 0; c < 3; ++c) {
            for (int i = 0; i < 65536; ++i) {
                float v = (float)i / 65535.0f;
                // standardMTF expected: x, m, lo, hi
                float stretched = standardMTF(v, stf[c].midtones, stf[c].shadows, stf[c].highlights);
                if (inverted) stretched = 1.0f - stretched;
                luts[c][i] = static_cast<uint8_t>(std::clamp(stretched * 255.0f, 0.0f, 255.0f));
            }
        }
    } else {
        // Linear
        for (int c = 0; c < 3; ++c) {
            for (int i = 0; i < 65536; ++i) {
                float v = (float)i / 65535.0f;
                if (inverted) v = 1.0f - v;
                luts[c][i] = static_cast<uint8_t>(std::clamp(v * 255.0f, 0.0f, 255.0f));
            }
        }
    }
    
    float dx = (float)w / outW;
    float dy = (float)h / outH;
    
    #pragma omp parallel for
    for (int y = 0; y < outH; ++y) {
        int srcY = (int)(y * dy);
        if(srcY >= m_height) srcY = m_height - 1;
        
        QRgb* scanLine = (QRgb*)img.scanLine(y);
        for (int x = 0; x < outW; ++x) {
            int srcX = (int)(x * dx);
            if(srcX >= m_width) srcX = m_width - 1;
            
            size_t idx = (static_cast<size_t>(srcY) * m_width + srcX) * m_channels;
            
            float r = m_data[idx];
            float g = (m_channels >= 2) ? m_data[idx+1] : r;
            float b = (m_channels >= 3) ? m_data[idx+2] : r;
            
            // Map via LUT
            uint8_t R = luts[0][(int)(std::clamp(r, 0.0f, 1.0f) * 65535.0f)];
            uint8_t G = luts[1][(int)(std::clamp(g, 0.0f, 1.0f) * 65535.0f)];
            uint8_t B = luts[2][(int)(std::clamp(b, 0.0f, 1.0f) * 65535.0f)];
            
            scanLine[x] = qRgb(R, G, B);
        }
    }
    
    return img;
}

// WCS Reframing Helper

void ImageBuffer::reframeWCS(const QTransform& trans, [[maybe_unused]] int oldWidth, [[maybe_unused]] int oldHeight) {
    if (m_meta.ra == 0 && m_meta.dec == 0) return;
    double crpix1_0 = m_meta.crpix1 - 1.0;
    double crpix2_0 = m_meta.crpix2 - 1.0;
    
    QPointF pOld(crpix1_0, crpix2_0);
    QPointF pNew = trans.map(pOld);
    
    m_meta.crpix1 = pNew.x() + 1.0;
    m_meta.crpix2 = pNew.y() + 1.0;

    // 2. Update CD Matrix
    // CD_new = CD_old * T^-1
    bool invertible = false;
    QTransform inv = trans.inverted(&invertible);
    if (!invertible) return;
    
    // Matrix multiplication:
    // [ cd11 cd12 ]   [ m11 m12 ]
    // [ cd21 cd22 ] * [ m21 m22 ]
    
    double old_cd11 = m_meta.cd1_1;
    double old_cd12 = m_meta.cd1_2;
    double old_cd21 = m_meta.cd2_1;
    double old_cd22 = m_meta.cd2_2;
    
    m_meta.cd1_1 = old_cd11 * inv.m11() + old_cd12 * inv.m21();
    m_meta.cd1_2 = old_cd11 * inv.m12() + old_cd12 * inv.m22();
    m_meta.cd2_1 = old_cd21 * inv.m11() + old_cd22 * inv.m21();
    m_meta.cd2_2 = old_cd21 * inv.m12() + old_cd22 * inv.m22();

    syncWcsToHeaders();
}

// Geometric Ops
void ImageBuffer::crop(int x, int y, int w, int h) {
    WriteLock lock(this);  // Thread-safe write access
    
    if (m_data.empty()) return;

    int oldW = m_width;
    int oldH = m_height;
    
    // Bounds check
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > m_width) w = m_width - x;
    if (y + h > m_height) h = m_height - y;
    
    if (w <= 0 || h <= 0) return;
    
    std::vector<float> newData(w * h * m_channels);
    
    for (int ry = 0; ry < h; ++ry) {
        int srcY = y + ry;
        int srcIdxStart = (srcY * m_width + x) * m_channels;
        int destIdxStart = (ry * w) * m_channels;
        int copySize = w * m_channels;
        
        for (int k = 0; k < copySize; ++k) {
             newData[destIdxStart + k] = m_data[srcIdxStart + k];
        }
    }
    
    m_width = w;
    m_height = h;
    m_data = newData;

    // Update WCS
    // Crop: translate(-x, -y)
    QTransform t;
    t.translate(-x, -y);
    reframeWCS(t, oldW, oldH);
}

void ImageBuffer::rotate(float angleDegrees) {
    WriteLock lock(this);  // Thread-safe write access
    
    if (m_data.empty()) return;
    if (std::abs(angleDegrees) < 0.1f) return;
    
    int oldW = m_width;
    int oldH = m_height;
    
    // Convert to radians
    float theta = -angleDegrees * 3.14159265f / 180.0f; // Negative to match typical image coord rotation
    float cosT = std::cos(theta);
    float sinT = std::sin(theta);
    
    // New dimensions (bounding box)
    // Corners: (0,0), (w,0), (0,h), (w,h)
    float x0 = 0, y0 = 0;
    float x1 = m_width, y1 = 0;
    float x2 = 0, y2 = m_height;
    float x3 = m_width, y3 = m_height;
    
    auto rotX = [&](float x, float y) { return x*cosT - y*sinT; };
    auto rotY = [&](float x, float y) { return x*sinT + y*cosT; };
    
    float rx0 = rotX(x0,y0), ry0 = rotY(x0,y0);
    float rx1 = rotX(x1,y1), ry1 = rotY(x1,y1);
    float rx2 = rotX(x2,y2), ry2 = rotY(x2,y2);
    float rx3 = rotX(x3,y3), ry3 = rotY(x3,y3);
    
    float minX = std::min({rx0, rx1, rx2, rx3});
    float maxX = std::max({rx0, rx1, rx2, rx3});
    float minY = std::min({ry0, ry1, ry2, ry3});
    float maxY = std::max({ry0, ry1, ry2, ry3});
    
    int newW = static_cast<int>(std::ceil(maxX - minX));
    int newH = static_cast<int>(std::ceil(maxY - minY));
    
    std::vector<float> newData(newW * newH * m_channels, 0.0f);
    
    float centerX = m_width / 2.0f;
    float centerY = m_height / 2.0f;
    float newCenterX = newW / 2.0f;
    float newCenterY = newH / 2.0f;
    

    #pragma omp parallel for
    for (int y = 0; y < newH; ++y) {
        for (int x = 0; x < newW; ++x) {
            // Inverse mapping
            float dx = x - newCenterX;
            float dy = y - newCenterY;
            
            // Rotate back by -theta (so +theta in matrix) to find src
            float srcX = dx * std::cos(-theta) - dy * std::sin(-theta) + centerX;
            float srcY = dx * std::sin(-theta) + dy * std::cos(-theta) + centerY;
            
            // Bilinear Interpolation
            if (srcX >= 0 && srcX < m_width - 1 && srcY >= 0 && srcY < m_height - 1) {
                int px = static_cast<int>(srcX);
                int py = static_cast<int>(srcY);
                float fx = srcX - px;
                float fy = srcY - py;
                
                int idx00 = (py * m_width + px) * m_channels;
                int idx01 = ((py) * m_width + (px+1)) * m_channels;
                int idx10 = ((py+1) * m_width + px) * m_channels;
                int idx11 = ((py+1) * m_width + (px+1)) * m_channels;
                
                for (int c = 0; c < m_channels; ++c) {
                    float v00 = m_data[idx00 + c];
                    float v01 = m_data[idx01 + c];
                    float v10 = m_data[idx10 + c];
                    float v11 = m_data[idx11 + c];
                    
                    float top = v00 * (1 - fx) + v01 * fx;
                    float bot = v10 * (1 - fx) + v11 * fx;
                    float val = top * (1 - fy) + bot * fy;
                    
                    newData[(y * newW + x) * m_channels + c] = val;
                }
            }
        }
    }
    
    m_width = newW;
    m_height = newH;
    m_data = newData;

    // Update WCS
    // Rotates around CENTER of image.
    // T = T_newCenter * R * T_-oldCenter
    QTransform t;
    t.translate(centerX, centerY);
    t.rotate(angleDegrees);
    t.translate(-centerX, -centerY);
    // Let's build explicitly:
    QTransform wcsTrans;
    wcsTrans.translate(newCenterX, newCenterY);
    wcsTrans.rotate(angleDegrees);
    wcsTrans.translate(-centerX, -centerY);
    
    reframeWCS(wcsTrans, oldW, oldH);
}


void ImageBuffer::cropRotated(float cx, float cy, float w, float h, float angleDegrees) {
    WriteLock lock(this);  // Thread-safe write access
    if (m_data.empty()) return;
    if (w <= 1 || h <= 1) return;

    int oldW = m_width;
    int oldH = m_height;

    // Output size is fixed to w, h
    int outW = static_cast<int>(w);
    int outH = static_cast<int>(h);
    
    
    std::vector<float> newData(outW * outH * m_channels);
    
    float theta = angleDegrees * 3.14159265f / 180.0f; // Radians (Positive for visual CW match)
    float cosT = std::cos(theta);
    float sinT = std::sin(theta);
    
    float halfW = w / 2.0f;
    float halfH = h / 2.0f;
    
    // Center of source: cx, cy
    
    #pragma omp parallel for
    for (int y = 0; y < outH; ++y) {
        for (int x = 0; x < outW; ++x) {
            // Coord relative to center of new image
            float dx = x - halfW;
            float dy = y - halfH;
            
            // Rotate back to align with source axes
            float srcDX = dx * cosT - dy * sinT;
            float srcDY = dx * sinT + dy * cosT;
            
            // Add source center
            float srcX = cx + srcDX;
            float srcY = cy + srcDY;
            
            // Bilinear Interp
            if (srcX >= 0 && srcX < m_width - 1 && srcY >= 0 && srcY < m_height - 1) {
                int px = static_cast<int>(srcX);
                int py = static_cast<int>(srcY);
                float fx = srcX - px;
                float fy = srcY - py;
                
                int idx00 = (py * m_width + px) * m_channels;
                int idx01 = ((py) * m_width + (px+1)) * m_channels;
                int idx10 = ((py+1) * m_width + px) * m_channels;
                int idx11 = ((py+1) * m_width + (px+1)) * m_channels;
                
                for (int c = 0; c < m_channels; ++c) {
                    float v00 = m_data[idx00 + c];
                    float v01 = m_data[idx01 + c];
                    float v10 = m_data[idx10 + c];
                    float v11 = m_data[idx11 + c];
                    
                    float top = v00 * (1 - fx) + v01 * fx;
                    float bot = v10 * (1 - fx) + v11 * fx;
                    float val = top * (1 - fy) + bot * fy;
                    
                    newData[(y * outW + x) * m_channels + c] = val;
                }
            } else {
                // Background color (Black)
                for (int c = 0; c < m_channels; ++c) {
                    newData[(y * outW + x) * m_channels + c] = 0.0f;
                }
            }
        }
    }
    
    m_width = outW;
    m_height = outH;
    m_data = newData;
    
    // WCS Transform: maps from new image coordinates to source image coordinates
    // Sequence: translate to center of source, rotate, translate to center of destination
    // This is the FORWARD mapping: dest_pixel -> source_pixel
    // reframeWCS will invert it for the CD matrix, which is what we want
    QTransform wcsTrans;
    wcsTrans.translate(cx, cy);          // Center at source center
    wcsTrans.rotate(-angleDegrees);      // Rotate by negative angle (inverse rotation)
    wcsTrans.translate(-halfW, -halfH);  // Translate from destination center
    
    reframeWCS(wcsTrans, oldW, oldH);
}

float ImageBuffer::getPixelValue(int x, int y, int c) const {
    ReadLock lock(this);
    if (m_data.empty()) return 0.0f; // SWAP SAFETY
    if (x < 0 || x >= m_width || y < 0 || y >= m_height || c < 0 || c >= m_channels) return 0.0f;
    return m_data[(static_cast<size_t>(y) * m_width + x) * m_channels + c];
}

float ImageBuffer::getPixelFlat(size_t index, int c) const {
    ReadLock lock(this);
    if (m_data.empty()) return 0.0f; // SWAP SAFETY
    if (m_channels == 1) {
        if (index >= m_data.size()) return 0.0f;
        return m_data[index];
    }
    size_t idx = index * m_channels + c;
    if (idx >= m_data.size()) return 0.0f;
    return m_data[idx];
}

float ImageBuffer::getChannelMedian(int channelIndex) const {
    ReadLock lock(this);
    if (m_data.empty()) return 0.0f;
    
    // Create view of channel data
    std::vector<float> chData;
    chData.reserve(m_data.size() / m_channels);
    int ch = m_channels;
    for (size_t i = channelIndex; i < m_data.size(); i += ch) {
        chData.push_back(m_data[i]);
    }
    
    return RobustStatistics::getMedian(chData);
}

float ImageBuffer::getAreaMean(int x, int y, int w, int h, int c) const {
    qDebug() << "[ImageBuffer::getAreaMean] Request:" << x << y << w << h << "ch:" << c << "buf:" << m_name << (void*)this;
    ReadLock lock(this);
    qDebug() << "[ImageBuffer::getAreaMean] Lock acquired. Data size:" << m_data.size() << "Width:" << m_width << "Height:" << m_height;
    // 1. Intersect requested rect with image bounds
    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(m_width, x + w);
    int y1 = std::min(m_height, y + h);
    
    // 2. Check for empty intersection
    if (x1 <= x0 || y1 <= y0) return 0.0f;
    
    if (m_data.empty()) return 0.0f;
    
    // 3. Compute mean over the valid intersection
    double sum = 0.0;
    long count = static_cast<long>(x1 - x0) * (y1 - y0);
    
    #pragma omp parallel for reduction(+:sum)
    for (int iy = y0; iy < y1; ++iy) {
        for (int ix = x0; ix < x1; ++ix) {
             size_t idx = (static_cast<size_t>(iy) * m_width + ix) * m_channels + c;
             sum += m_data[idx];
        }
    }
    return (count > 0) ? (float)(sum / count) : 0.0f;
}

void ImageBuffer::computeClippingStats(long& lowClip, long& highClip) const {
    ReadLock lock(this);
    lowClip = 0;
    highClip = 0;
    
    long tempLow = 0;
    long tempHigh = 0;
    size_t n = m_data.size();
    
    #pragma omp parallel for reduction(+:tempLow, tempHigh)
    for (size_t i = 0; i < n; ++i) {
        float v = m_data[i];
        if (v <= 0.0f) tempLow++;
        else if (v >= 1.0f) tempHigh++;
    }
    
    lowClip = tempLow;
    highClip = tempHigh;
}

std::vector<std::vector<int>> ImageBuffer::computeHistogram(int bins) const {
    ReadLock lock(this);
    if (m_data.empty() || bins <= 0) return {};
    
    int numThreads = omp_get_max_threads();
    if (numThreads < 1) numThreads = 1;
    
    std::vector<std::vector<std::vector<int>>> localHists(numThreads, 
        std::vector<std::vector<int>>(m_channels, std::vector<int>(bins, 0)));
    
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        #pragma omp for
        for (long long i = 0; i < (long long)m_data.size(); ++i) { // Process all pixels
            int c = i % m_channels;
            float v = m_data[i];
            
            // Fast clamp
            if (v < 0.0f) v = 0.0f;
            else if (v > 1.0f) v = 1.0f;
            
            int b = static_cast<int>(v * (bins - 1) + 0.5f);
            localHists[tid][c][b]++;
        }
    }
    
    std::vector<std::vector<int>> hist(m_channels, std::vector<int>(bins, 0));
    for (int t = 0; t < numThreads; ++t) {
        for (int c = 0; c < m_channels; ++c) {
            for (int b = 0; b < bins; ++b) {
                hist[c][b] += localHists[t][c][b];
            }
        }
    }
    return hist;
}

void ImageBuffer::rotate90() {
    WriteLock lock(this);  // Thread-safe write access
    
    if (m_data.empty()) return;
    
    int oldW = m_width;
    int oldH = m_height;
    int ch = m_channels;
    std::vector<float> newData(oldW * oldH * ch);
    
    #pragma omp parallel for
    for (int y = 0; y < oldH; ++y) {
        for (int x = 0; x < oldW; ++x) {
            int newX = oldH - 1 - y;
            int newY = x;
            int oldIdx = (y * oldW + x) * ch;
            int newIdx = (newY * oldH + newX) * ch;
            for (int c = 0; c < ch; ++c) {
                newData[newIdx + c] = m_data[oldIdx + c];
            }
        }
    }
    
    m_width = oldH;
    m_height = oldW;
    m_data = std::move(newData);

    // Update WCS for Rotate 90 CW
    // T: (x,y) -> (H-1-y, x)
    // T = Translate(H-1, 0) * Rotate(90)
    
    QTransform t;
    t.translate(oldH - 1, 0);
    t.rotate(90);
    
    reframeWCS(t, oldW, oldH);
}

void ImageBuffer::rotate180() {
    WriteLock lock(this);  // Thread-safe write access
    
    if (m_data.empty()) return;
    
    int h = m_height;
    int w = m_width;
    int ch = m_channels;
    
    #pragma omp parallel for
    for (int y = 0; y < h / 2; ++y) {
        int y2 = h - 1 - y;
        for (int x = 0; x < w; ++x) {
            int x2 = w - 1 - x;
            int idx1 = (y * w + x) * ch;
            int idx2 = (y2 * w + x2) * ch;
            for (int c = 0; c < ch; ++c) {
                std::swap(m_data[idx1 + c], m_data[idx2 + c]);
            }
        }
    }
    
    // Handle middle row if height is odd
    if (h % 2 != 0) {
        int y = h / 2;
        for (int x = 0; x < w / 2; ++x) {
            int x2 = w - 1 - x;
            int idx1 = (y * w + x) * ch;
            int idx2 = (y * w + x2) * ch;
            for (int c = 0; c < ch; ++c) {
                std::swap(m_data[idx1 + c], m_data[idx2 + c]);
            }
        }
    }
    
    // Update WCS
    // 180 Rotation: (x,y) -> (W-1-x, H-1-y)
    // T = Translate(W-1, H-1) * Rotate(180)
    QTransform t;
    t.translate(w - 1, h - 1);
    t.rotate(180);
    reframeWCS(t, w, h);
}

void ImageBuffer::rotate270() {
    WriteLock lock(this);  // Thread-safe write access
    
    if (m_data.empty()) return;
    
    int oldW = m_width;
    int oldH = m_height;
    int ch = m_channels;
    std::vector<float> newData(oldW * oldH * ch);
    
    #pragma omp parallel for
    for (int y = 0; y < oldH; ++y) {
        for (int x = 0; x < oldW; ++x) {
            int newX = y;
            int newY = oldW - 1 - x;
            int oldIdx = (y * oldW + x) * ch;
            int newIdx = (newY * oldH + newX) * ch;
            for (int c = 0; c < ch; ++c) {
                newData[newIdx + c] = m_data[oldIdx + c];
            }
        }
    }
    
    m_width = oldH;
    m_height = oldW;
    m_data = std::move(newData);
    
    // Update WCS
    // Rotate 270 CW (90 CCW)
    // x' = y
    // y' = W - 1 - x
    // T = Translate(0, W-1) * Rotate(270)
    QTransform t;
    t.translate(0, oldW - 1);
    t.rotate(270);
    
    reframeWCS(t, oldW, oldH);
}


void ImageBuffer::mirrorX() {
    WriteLock lock(this);  // Thread-safe write access
    
    if (m_data.empty()) return;
    
    // Capture old dimensions before mod (though mirror doesn't change W/H)
    int w = m_width;
    int h = m_height;
    
    #pragma omp parallel for
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width / 2; ++x) {
            int x2 = m_width - 1 - x;
            
            size_t idx1 = (y * m_width + x) * m_channels;
            size_t idx2 = (y * m_width + x2) * m_channels;
            
            for (int c = 0; c < m_channels; ++c) {
                std::swap(m_data[idx1 + c], m_data[idx2 + c]);
            }
        }
    }
    
    // Update WCS
    // Mirror X (Horizontal): x' = W - 1 - x, y' = y
    // T = Translate(W-1, 0) * Scale(-1, 1)
    QTransform t;
    t.translate(w - 1, 0);
    t.scale(-1, 1);
    reframeWCS(t, w, h);
}

void ImageBuffer::syncWcsToHeaders() {
    // Helper lambda to set or add a key
    auto setKey = [&](const QString& key, double val, const QString& comment) {
        bool found = false;
        QString valStr = QString::number(val, 'f', 9);
        
        for (auto& card : m_meta.rawHeaders) {
            if (card.key == key) {
                card.value = valStr;
                found = true;
                break;
            }
        }
        if (!found) {
            m_meta.rawHeaders.push_back({key, valStr, comment});
        }
    };
    
    setKey("CRPIX1", m_meta.crpix1, "Reference pixel axis 1");
    setKey("CRPIX2", m_meta.crpix2, "Reference pixel axis 2");
    
    setKey("CD1_1", m_meta.cd1_1, "PC matrix 1_1");
    setKey("CD1_2", m_meta.cd1_2, "PC matrix 1_2");
    setKey("CD2_1", m_meta.cd2_1, "PC matrix 2_1");
    setKey("CD2_2", m_meta.cd2_2, "PC matrix 2_2");
}



void ImageBuffer::mirrorY() {
    WriteLock lock(this);  // Thread-safe write access
    
    if (m_data.empty()) return;
    
    int h = m_height;
    int w = m_width;
    int ch = m_channels;
    
    #pragma omp parallel for
    for (int y = 0; y < h / 2; ++y) {
        int y2 = h - 1 - y;
        for (int x = 0; x < w; ++x) {
            int idx1 = (y * w + x) * ch;
            int idx2 = (y2 * w + x) * ch;
            for (int c = 0; c < ch; ++c) {
                std::swap(m_data[idx1 + c], m_data[idx2 + c]);
            }
        }
    }
    
    // Update WCS
    // Mirror Y (Vertical): x' = x, y' = H - 1 - y
    // T = Translate(0, H-1) * Scale(1, -1)
    QTransform t;
    t.translate(0, h - 1);
    t.scale(1, -1);
    reframeWCS(t, w, h);
}

void ImageBuffer::multiply(float factor) {
    WriteLock lock(this);  // Thread-safe write access
    
    if (m_data.empty()) return;
    #pragma omp parallel for
    for (size_t i = 0; i < m_data.size(); ++i) {
        m_data[i] = std::max(0.0f, std::min(1.0f, m_data[i] * factor));
    }
}

void ImageBuffer::subtract(float r, float g, float b) {
    WriteLock lock(this);  // Thread-safe write access
    
    if (m_data.empty()) return;
    
    int ch = m_channels;
    long total = static_cast<long>(m_width) * m_height;
    
    #pragma omp parallel for
    for (long i = 0; i < total; ++i) {
        size_t idx = i * ch;
        if (ch == 1) {
            m_data[idx] = std::max(0.0f, m_data[idx] - r); // Use r for mono
        } else {
            m_data[idx + 0] = std::max(0.0f, m_data[idx + 0] - r);
            m_data[idx + 1] = std::max(0.0f, m_data[idx + 1] - g);
            m_data[idx + 2] = std::max(0.0f, m_data[idx + 2] - b);
        }
    }
}

float ImageBuffer::getChannelMAD(int channelIndex, float median) const {
    if (m_data.empty()) return 0.0f;
    
    // Extract channel data to pass to RobustStatistics
    // (We could optimize RobustStatistics to take stride, but copying is acceptable for O(N))
    std::vector<float> chData;
    chData.reserve(m_data.size() / m_channels);
    int ch = m_channels;
    for (size_t i = channelIndex; i < m_data.size(); i += ch) {
        chData.push_back(m_data[i]);
    }
    
    return RobustStatistics::getMAD(chData, median);
}

float ImageBuffer::getRobustMedian(int channelIndex, float t0, float t1) const {
    if (m_data.empty()) return 0.0f;
    
    std::vector<float> chData;
    chData.reserve(m_data.size() / m_channels);
    int ch = m_channels;
    for (size_t i = channelIndex; i < m_data.size(); i += ch) {
        chData.push_back(m_data[i]);
    }
    
    float med = RobustStatistics::getMedian(chData);
    float mad = RobustStatistics::getMAD(chData, med);
    
    float sigma = 1.4826f * mad;
    float lower = med + t0 * sigma; 
    float upper = med + t1 * sigma;

    // Filter
    std::vector<float> valid;
    valid.reserve(chData.size());
    for (float v : chData) {
        if (v >= lower && v <= upper) valid.push_back(v);
    }
    
    return RobustStatistics::getMedian(valid);
}




