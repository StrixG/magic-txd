// Utilities for interfacing between Qt and rwlib.
// Should not be included into the global headers, this is an on-demand component.

#include <algorithm>

inline QImage convertRWBitmapToQImage( const rw::Bitmap& rasterBitmap )
{
	rw::uint32 width, height;
	rasterBitmap.getSize(width, height);

	QImage texImage(width, height, QImage::Format::Format_ARGB32);

	// Copy scanline by scanline.
	for (rw::uint32 y = 0; y < height; y++)
	{
		uchar *scanLineContent = texImage.scanLine(y);

		QRgb *colorItems = (QRgb*)scanLineContent;

		for (rw::uint32 x = 0; x < width; x++)
		{
			QRgb *colorItem = (colorItems + x);

			unsigned char r, g, b, a;

			rasterBitmap.browsecolor(x, y, r, g, b, a);

			*colorItem = qRgba(r, g, b, a);
		}
	}

    return texImage;
}

inline QPixmap convertRWBitmapToQPixmap( const rw::Bitmap& rasterBitmap )
{
	return QPixmap::fromImage(
        convertRWBitmapToQImage( rasterBitmap )
    );
}

// Returns a sorted list of TXD platform names by importance.
template <typename stringListType>
inline std::vector <std::string> PlatformImportanceSort( MainWindow *mainWnd, const stringListType& platformNames )
{
    // Set up a weighted container of platform strings.
    struct weightedNode
    {
        double weight;
        std::string platName;

        inline bool operator < ( const weightedNode& right ) const
        {
            return ( this->weight > right.weight );
        }
    };

    size_t platCount = platformNames.size();

    // The result container.
    std::vector <std::string> sortedResult( platCount );

    weightedNode *nodes = new weightedNode[ platCount ];

    try
    {
        // Initialize the nodes.
        {
            auto iter = platformNames.begin();

            for ( size_t n = 0; n < platCount; n++ )
            {
                const auto& curPlatName = *iter++;

                weightedNode& curNode = nodes[ n ];

                curNode.platName = curPlatName;
                curNode.weight = 0;
            }
        }

        // Cache some things we are going to need.
        const char *rwRecTXDPlatform = NULL;
        QString rwActualPlatform;
        rw::LibraryVersion txdVersion;
        {
            if ( rw::TexDictionary *currentTXD = mainWnd->getCurrentTXD() )
            {
                rwRecTXDPlatform = currentTXD->GetRecommendedDriverPlatform();
                rwActualPlatform = mainWnd->GetCurrentPlatform();

                txdVersion = currentTXD->GetEngineVersion();
            }
        }

        // Process all platforms and store their rating.
        for ( size_t n = 0; n < platCount; n++ )
        {
            weightedNode& platNode = nodes[ n ];

            const std::string& name = platNode.platName;

            // If the platform is recommended by the internal RW toolchain, we want to put it up front.
            if ( rwRecTXDPlatform && name == rwRecTXDPlatform )
            {
                platNode.weight += 0.9;
            }

            // If the platform makes sense in the TXD's version configuration, it is kinda important.
            RwVersionSets::eDataType curDataType = RwVersionSets::dataIdFromEnginePlatformName( ansi_to_qt( name ) );

            if ( curDataType != RwVersionSets::RWVS_DT_NOT_DEFINED )
            {
                // Check whether this version makes sense in this platform.
                int setIndex, platIndex, dataTypeIndex;

                bool makesSense = mainWnd->versionSets.matchSet( txdVersion, curDataType, setIndex, platIndex, dataTypeIndex );

                if ( makesSense )
                {
                    // Honor it.
                    platNode.weight += 0.7;
                }
            }

            // If we match the current platform of the TXD, we are uber important!
            if ( rwActualPlatform.isEmpty() == false && rwActualPlatform == name.c_str() )
            {
                platNode.weight += 1.0;
            }
        }

        // Make the sorted thing.
        {
            std::sort( nodes, nodes + platCount );

            for ( size_t n = 0; n < platCount; n++ )
            {
                const weightedNode& curItem = nodes[ n ];

                sortedResult[ n ] = curItem.platName;
            }

            delete [] nodes;
        }
    }
    catch( ... )
    {
        delete [] nodes;

        throw;
    }

    return sortedResult;
}
