#include "util/FileUtils.h"

#include <QByteArray>
#include <QFile>

#include "util/Utf8Utils.h"

namespace {
constexpr int kSampleSize = 4096;
constexpr int kControlByteThreshold = 32;
constexpr int kDeleteByte = 127;
constexpr double kNonTextRatioThreshold = 0.15;
}  // namespace

namespace FileUtils {

auto isBinaryFile( const QString& filePath ) -> bool
{
    QFile file( filePath );
    if( !file.open( QIODevice::ReadOnly ) ) {
        return false;
    }
    QByteArray chunk = file.read( kSampleSize );
    file.close();

    if( chunk.isEmpty() ) {
        return false;
    }

    int nullBytes = 0;
    int controlCount = 0;

    for( char i : chunk ) {
        auto uc = static_cast<unsigned char>( i );
        if( uc == '\0' ) {
            nullBytes++;
        } else if( uc < kControlByteThreshold ) {
            if( uc != '\t' && uc != '\n' && uc != '\r' ) {
                controlCount++;
            }
        } else if( uc == kDeleteByte ) {
            controlCount++;
        }
    }

    if( nullBytes > 0 ) {
        return true;
    }

    int invalidUtf8Count = 0;
    int i = 0;
    while( i < chunk.size() ) {
        auto b1 = static_cast<unsigned char>( chunk.at( i ) );
        if( b1 < 128 ) {
            i++;
            continue;
        }

        int seq_len = Utf8Utils::sequenceLength( b1 );
        if( seq_len == 1 ) {
            invalidUtf8Count++;
            i++;
            continue;
        }

        if( i + seq_len > chunk.size() ) {
            break;
        }

        bool valid_seq = true;
        for( int j = 1; j < seq_len; ++j ) {
            auto bj = static_cast<unsigned char>( chunk.at( i + j ) );
            if( !Utf8Utils::isContinuationByte( bj ) ) {
                valid_seq = false;
                break;
            }
        }

        if( !valid_seq ) {
            invalidUtf8Count++;
            i++;
        } else {
            i += seq_len;
        }
    }

    double nonTextRatio = static_cast<double>( controlCount + invalidUtf8Count ) / chunk.size();
    return nonTextRatio > kNonTextRatioThreshold;
}

}  // namespace FileUtils
