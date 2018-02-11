#include "mainwindow.h"
#include <QtWidgets/QApplication>
#include "styles.h"
#include <QtCore/QTimer>
#include "resource.h"

#include <NativeExecutive/CExecutiveManager.h>

#include <QtCore/QtPlugin>

#ifdef _WIN32
#include <Windows.h>
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#elif defined(__linux__)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
#endif //CROSS PLATFORM CODE

#include <QtGui/QImageWriter>

struct ScopedSystemEventFilter
{
    inline ScopedSystemEventFilter(QObject *receiver, QEvent *evt)
    {
        _currentFilter = this;

        QWidget *theWidget = NULL;

        this->evt = NULL;
        this->handlerWidget = NULL;

        if ( receiver )
        {
            if (receiver->isWidgetType())
            {
                theWidget = (QWidget*)receiver;
            }
        }

        if (theWidget)
        {
            // We are not there yet.
            // Check whether this widget supports system event signalling.
            SystemEventHandlerWidget *hWidget = dynamic_cast <SystemEventHandlerWidget*> (theWidget);

            if (hWidget)
            {
                hWidget->beginSystemEvent(evt);

                this->evt = evt;
                this->handlerWidget = hWidget;
            }
        }
    }

    inline ~ScopedSystemEventFilter(void)
    {
        if (handlerWidget)
        {
            handlerWidget->endSystemEvent(evt);
        }

        _currentFilter = NULL;
    }

    static ScopedSystemEventFilter *_currentFilter;

    QEvent *evt;
    SystemEventHandlerWidget *handlerWidget;
};

ScopedSystemEventFilter* ScopedSystemEventFilter::_currentFilter = NULL;

SystemEventHandlerWidget::~SystemEventHandlerWidget(void)
{
    ScopedSystemEventFilter *sysEvtFilter = ScopedSystemEventFilter::_currentFilter;

    if (sysEvtFilter)
    {
        if (sysEvtFilter->handlerWidget = this)
        {
            sysEvtFilter->handlerWidget = NULL;
            sysEvtFilter->evt = NULL;
        }
    }
}

struct MagicTXDApplication : public QApplication
{
    inline MagicTXDApplication(int& argc, char *argv[]) : QApplication(argc, argv)
    {
        return;
    }

    bool notify(QObject *receiver, QEvent *evt) override
    {
        ScopedSystemEventFilter filter(receiver, evt);

        return QApplication::notify(receiver, evt);
    }
};

// Main window factory.
mainWindowFactory_t mainWindowFactory;

struct mainWindowConstructor
{
    QString appPath;
    rw::Interface *rwEngine;
    CFileSystem *fsHandle;

    inline mainWindowConstructor(QString&& appPath, rw::Interface *rwEngine, CFileSystem *fsHandle)
        : appPath(appPath), rwEngine(rwEngine), fsHandle(fsHandle)
    {}

    inline MainWindow* Construct(void *mem) const
    {
        return new (mem) MainWindow(appPath, rwEngine, fsHandle);
    }
};

struct defaultMemAlloc
{
    static void* Allocate(size_t memSize)
    {
        return malloc(memSize);
    }

    static void Free(void *mem, size_t memSize)
    {
        free(mem);
    }
};

// Main window plugin entry points.
extern void InitializeRWFileSystemWrap(void);
extern void InitializeTaskCompletionWindowEnv( void );
extern void InitializeSerializationStorageEnv( void );
extern void InitializeMainWindowSerializationBlock( void );
extern void InitializeMagicLanguages( void );
extern void InitializeHelperRuntime( void );
extern void InitializeMainWindowHelpEnv( void );
extern void InitializeTextureAddDialogEnv( void );
extern void InitializeExportAllWindowSerialization( void );
extern void InitializeMassconvToolEnvironment(void);
extern void InitializeMassExportToolEnvironment( void );
extern void InitializeMassBuildEnvironment( void );
extern void InitializeGUISerialization(void);
extern void InitializeStreamCompressionEnvironment( void );

static defaultMemAlloc _factMemAlloc;

extern void DbgHeap_Validate( void );

static void important_message( const char *msg, const char *title )
{
#ifdef _WIN32
    MessageBoxA( nullptr, msg, title, MB_OK );
#elif defined(__linux__)
    printf( "%s\n", msg );
#endif //CROSS PLATFORM CODE
}

int main(int argc, char *argv[])
{
    // Initialize all main window plugins.
    InitializeRWFileSystemWrap();
    InitializeTaskCompletionWindowEnv();
    InitializeSerializationStorageEnv();
    InitializeMainWindowSerializationBlock();
    InitializeMagicLanguages();
    InitializeHelperRuntime();
    InitializeMainWindowHelpEnv();
    InitializeTextureAddDialogEnv();
    InitializeExportAllWindowSerialization();
    InitializeMassconvToolEnvironment();
    InitializeMassExportToolEnvironment();
    InitializeMassBuildEnvironment();
    InitializeGUISerialization();
    InitializeStreamCompressionEnvironment();

    int iRet = -1;

    try
    {
        // Initialize the RenderWare engine.
        rw::LibraryVersion engineVersion;

        // This engine version is the default version we create resources in.
        // Resources can change their version at any time, so we do not have to change this.
        engineVersion.rwLibMajor = 3;
        engineVersion.rwLibMinor = 6;
        engineVersion.rwRevMajor = 0;
        engineVersion.rwRevMinor = 3;

        rw::Interface *rwEngine = rw::CreateEngine( engineVersion );

        if ( rwEngine == NULL )
        {
            throw std::exception(); // "failed to initialize the RenderWare engine"
        }

        try
        {
            NativeExecutive::CExecutiveManager *nativeExec = (NativeExecutive::CExecutiveManager*)rw::GetThreadingNativeManager( rwEngine );

            // Set some typical engine properties.
            // The MainWindow will override these.
            rwEngine->SetIgnoreSerializationBlockRegions( true );
            rwEngine->SetIgnoreSecureWarnings( false );

            rwEngine->SetWarningLevel( 3 );

            rwEngine->SetCompatTransformNativeImaging( true );
            rwEngine->SetPreferPackedSampleExport( true );

            rwEngine->SetDXTRuntime( rw::DXTRUNTIME_SQUISH );
            rwEngine->SetPaletteRuntime( rw::PALRUNTIME_PNGQUANT );

            // Give RenderWare some info about us!
            rw::softwareMetaInfo metaInfo;
            metaInfo.applicationName = "Magic.TXD";
            metaInfo.applicationVersion = MTXD_VERSION_STRING;
            metaInfo.description = "by DK22Pac and The_GTA (https://github.com/quiret/magic-txd)";

            rwEngine->SetApplicationInfo( metaInfo );

            // Initialize the filesystem.
            fs_construction_params fsParams;
            fsParams.nativeExecMan = nativeExec;

            CFileSystem *fsHandle = CFileSystem::Create( fsParams );

            if ( !fsHandle )
            {
                throw std::exception(); // "failed to initialize the FileSystem module"
            }

            try
            {
                // removed library path stuff, because we statically link.

                MagicTXDApplication a(argc, argv);
                {
                    QString styleSheet = styles::get(a.applicationDirPath(), "resources/dark.shell");

                    if ( styleSheet.isEmpty() )
                    {
                        important_message(
                            "Failed to load stylesheet resource \"resources/dark.shell\".\n" \
                            "Please verify whether you have installed Magic.TXD correctly!",
                            "Error"
                        );

                        // Even without stylesheet, we allow launching Magic.TXD.
                        // The user gets what he asked for.
                    }
                    else
                    {
                        a.setStyleSheet(styleSheet);
                    }
                }
                mainWindowConstructor wnd_constr(a.applicationDirPath(), rwEngine, fsHandle);

                MainWindow *w = mainWindowFactory.ConstructTemplate(_factMemAlloc, wnd_constr);

                try
                {
                    w->setWindowIcon(QIcon(w->makeAppPath("resources/icons/stars.png")));
                    w->show();

                    w->launchDetails();

                    QApplication::processEvents();

                    QStringList appargs = a.arguments();

                    if (appargs.size() >= 2) {
                        QString txdFileToBeOpened = appargs.at(1);
                        if (!txdFileToBeOpened.isEmpty()) {
                            w->openTxdFile(txdFileToBeOpened);

                            w->adjustDimensionsByViewport();
                        }
                    }

                    // Try to catch some known C++ exceptions and display things for them.
                    try
                    {
                        iRet = a.exec();
                    }
                    catch( rw::RwException& except )
                    {
                        std::string errMsg = "uncaught RenderWare exception: " + except.message;

                        important_message(
                            errMsg.c_str(),
                            "Uncaught C++ Exception"
                        );

                        // Continue execution.
                        iRet = -1;
                    }
                    catch( std::exception& except )
                    {
                        std::string errMsg = std::string( "uncaught C++ STL exception: " ) + except.what();

                        important_message(
                            errMsg.c_str(),
                            "Uncaught C++ Exception"
                        );

                        // Continue execution.
                        iRet = -2;
                    }
                }
                catch( ... )
                {
                    mainWindowFactory.Destroy( _factMemAlloc, w );

                    throw;
                }

                mainWindowFactory.Destroy(_factMemAlloc, w);
            }
            catch( ... )
            {
                CFileSystem::Destroy( fsHandle );

                throw;
            }

            CFileSystem::Destroy( fsHandle );
        }
        catch( ... )
        {
            rw::DeleteEngine( rwEngine );

            throw;
        }

        rw::DeleteEngine( rwEngine );
    }
    catch( rw::RwException& except )
    {
        std::string errMsg = "uncaught RenderWare error during init: " + except.message;

        important_message(
            errMsg.c_str(),
            "Uncaught C++ Exception"
        );

        // Just continue.
    }
    catch( std::exception& except )
    {
        std::string errMsg = std::string( "uncaught C++ STL error during init: " ) + except.what();

        important_message(
            errMsg.c_str(),
            "Uncaught C++ Exception"
        );

        // Just continue.
    }
    catch( ... )
    {
        important_message(
            "Magic.TXD has encountered an unknown C++ exception and was forced to close. Please report this to the developers with appropriate steps to reproduce.",
            "Uncaught C++ Exception"
        );

        // Continue for safe quit.
    }

    DbgHeap_Validate();

    return iRet;
}
