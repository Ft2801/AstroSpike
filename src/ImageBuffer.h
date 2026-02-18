#ifndef IMAGEBUFFER_H
#define IMAGEBUFFER_H

#include <vector>
#include <QImage>
#include <QString>
#include <QMap>
#include <QVariant>
#include <QMutex>
#include <QReadWriteLock>
#include <QTransform>
#include <QDateTime>


class ImageBuffer {
public:
    ImageBuffer();
    ImageBuffer(int width, int height, int channels);
    ~ImageBuffer();
    
    // Custom copy to handle non-copyable mutex
    ImageBuffer(const ImageBuffer& other);
    ImageBuffer& operator=(const ImageBuffer& other);

    // Move is allowed
    ImageBuffer(ImageBuffer&& other) noexcept = default;
    ImageBuffer& operator=(ImageBuffer&& other) noexcept = default;

    // WCS Reframing Helper (Centralized logic for Geometry Ops)
    // Updates CRPIX and CD Matrix based on a linear transform
    void reframeWCS(const QTransform& trans, int oldWidth, int oldHeight);

    void setData(int width, int height, int channels, const std::vector<float>& data);
    void resize(int width, int height, int channels);
    
    // Raw Access
    // CRITICAL: Calling data() will trigger a swap-in if needed.
    // Ensure you hold a lock if needed, though data() itself handles the swap logic.
    const std::vector<float>& data() const;
    std::vector<float>& data();
    

    
    bool loadStandard(const QString& filePath);
    
    int width() const { Q_ASSERT(m_width > 0); return m_width; }
    int height() const { Q_ASSERT(m_height > 0); return m_height; }
    
    /**
     * @brief Load a rectangular region of the image
     */
    bool loadRegion(const QString& filePath, int x, int y, int w, int h, QString* errorMsg = nullptr);
    
    // Header Access
    QString getHeaderValue(const QString& key) const;

    enum BitDepth { Depth_8Int, Depth_16Int, Depth_32Int, Depth_32Float };
    enum DisplayMode { Display_Linear, Display_AutoStretch };
    int channels() const { Q_ASSERT(m_channels > 0); return m_channels; }
    size_t size() const { return static_cast<size_t>(m_width) * m_height * m_channels; }
    bool isValid() const { return !m_data.empty() && m_width > 0 && m_height > 0; }

    // Display
    // Replaces the boolean autostretch with mode and link option
    // Optional overrideLUT: if provided (size 3x65536), it is used instead of internal logic.
    QImage getDisplayImage(DisplayMode mode = Display_Linear, bool linked = true, const std::vector<std::vector<float>>* overrideLUT = nullptr, int maxWidth = 0, int maxHeight = 0, bool inverted = false, bool falseColor = false) const; 


    // Saving and Processing


    bool save(const QString& filePath, const QString& format, BitDepth depth, QString* errorMsg = nullptr) const;
    bool loadTiff32(const QString& filePath, QString* errorMsg = nullptr, QString* debugInfo = nullptr);

    
    

    // Geometric Ops
    void crop(int x, int y, int w, int h);
    void rotate(float angleDegrees); // Positive = Clockwise
    
    // Interactive Crop with Subpixel & Rotation
    void cropRotated(float cx, float cy, float w, float h, float angleDegrees);

    // New Geometry Tools
    void rotate90();   // CW
    void rotate180();
    void rotate270();  // CW (90 CCW)
    void mirrorX();    // Horizontal Flip
    void mirrorY();    // Vertical Flip


    // Math Ops
    void multiply(float factor);

    void applyWhiteBalance(float r, float g, float b);
    void subtract(float r, float g, float b); // Subtract offsets per channel (clamped to 0)
    
    // Stats Helper
    float getChannelMedian(int channelIndex) const;
    float getChannelMAD(int channelIndex, float median) const;
    float getRobustMedian(int channelIndex, float t0, float t1) const; 
    float getPixelValue(int x, int y, int c) const;
    float value(int x, int y, int c = 0) const { return getPixelValue(x, y, c); }
    float& value(int x, int y, int c = 0) { 
        return m_data[(static_cast<size_t>(y) * m_width + x) * m_channels + c]; 
    }
    float getPixelFlat(size_t index, int c) const;
    float getAreaMean(int x, int y, int w, int h, int c) const;

    struct Metadata {
        double focalLength = 0;
        double pixelSize = 0;
        double exposure = 0;
        double ra = 0;
        double dec = 0;
        
        // WCS Matrix
        double crpix1 = 0, crpix2 = 0;
        double cd1_1 = 0, cd1_2 = 0, cd2_1 = 0, cd2_2 = 0;
        
        // Additional WCS parameters
        QString ctype1;   // e.g., "RA---TAN", "RA---TAN-SIP"
        QString ctype2;   // e.g., "DEC--TAN", "DEC--TAN-SIP"
        double equinox = 2000.0;
        double lonpole = 180.0;
        double latpole = 0.0;
        
        // SIP Distortion Coefficients
        int sipOrderA = 0;   // A_ORDER
        int sipOrderB = 0;   // B_ORDER
        int sipOrderAP = 0;  // AP_ORDER (inverse)
        int sipOrderBP = 0;  // BP_ORDER (inverse)
        QMap<QString, double> sipCoeffs;  // "A_1_0" -> value, "B_2_1" -> value, etc.
        
        QString objectName;
        QString dateObs;
        QString filePath;      // Source file path for reference
        QString bitDepth;      // Original bit depth info
        bool isMono = false;   // True if originally mono image
        int64_t stackCount = 0; // Number of images combined
        double ccdTemp = 0.0;
        QString bayerPattern; // e.g. "RGGB", "GBRG" etc. from BAYERPAT header

        
        // Raw Header Storage (Key, Value, Comment)
        struct HeaderCard {
             QString key;
             QString value;
             QString comment;
        };
        std::vector<HeaderCard> rawHeaders;
        
    };

    void setMetadata(const Metadata& meta) { m_meta = meta; }
    const Metadata& metadata() const { return m_meta; }
    Metadata& metadata() { return m_meta; }
    
    // Computes clipping stats (pixels <= 0 and >= 1) - Parallelized
    void computeClippingStats(long& lowClip, long& highClip) const;

    // Histogram
    std::vector<std::vector<int>> computeHistogram(int bins = 256) const;

    // Name Tracking
    void setName(const QString& name) { m_name = name; }
    QString name() const { return m_name; }

    bool isModified() const { return m_modified; }
    void setModified(bool modified) { m_modified = modified; }
    
    // Synchronize WCS struct values back to rawHeaders vector
    void syncWcsToHeaders();
    
    // Thread Safety: Lock/Unlock for multi-threaded access
    // Usage: buffer.lockRead(); /* read data */ buffer.unlock();
    // Or:    buffer.lockWrite(); /* modify data */ buffer.unlock();
    void lockRead() const { if (!m_mutex) m_mutex = std::make_unique<QReadWriteLock>(QReadWriteLock::Recursive); m_mutex->lockForRead(); }
    void lockWrite() { if (!m_mutex) m_mutex = std::make_unique<QReadWriteLock>(QReadWriteLock::Recursive); m_mutex->lockForWrite(); }
    void unlock() const { if (m_mutex) m_mutex->unlock(); }
    
    // RAII Helper for automatic lock management
    // These ensure data is swapped-in before any access to m_data.
    class ReadLock {
    public:
        explicit ReadLock(const ImageBuffer* buf) : m_buf(buf) {
            if (m_buf) {
                // Swap-in BEFORE acquiring read lock (forceSwapIn needs write lock)
                // if (m_buf->m_isSwapped) {
                //     const_cast<ImageBuffer*>(m_buf)->forceSwapIn();
                // }
                m_buf->lockRead();
            }
        }
        ~ReadLock() { if (m_buf) m_buf->unlock(); }
        ReadLock(const ReadLock&) = delete;
        ReadLock& operator=(const ReadLock&) = delete;
    private:
        const ImageBuffer* m_buf;
    };
    
    class WriteLock {
    public:
        explicit WriteLock(ImageBuffer* buf) : m_buf(buf) {
            if (m_buf) {
                m_buf->lockWrite();
                // Swap-in AFTER acquiring write lock (we already hold it, call doSwapIn directly)
                // if (m_buf->m_isSwapped) {
                //     m_buf->doSwapIn();
                // }
            }
        }
        ~WriteLock() { if (m_buf) m_buf->unlock(); }
        WriteLock(const WriteLock&) = delete;
        WriteLock& operator=(const WriteLock&) = delete;
    private:
        ImageBuffer* m_buf;
    };

private:
    int m_width = 0;
    int m_height = 0;
    int m_channels = 1; 
    std::vector<float> m_data; // Interleaved 32-bit float (0.0 - 1.0)
    Metadata m_meta;
    QString m_name;
    bool m_modified = false;
    
    
    // Thread Safety: Read-Write lock for concurrent access
    // Multiple readers allowed, exclusive write access required
    mutable std::unique_ptr<QReadWriteLock> m_mutex;



    // Agile Autostretch (Display only)
    std::vector<float> computeAgileLUT(int channelIndex, float targetMedian = 0.25f);
    
public:
    
    
};

#endif // IMAGEBUFFER_H
