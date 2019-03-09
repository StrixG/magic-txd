// FileSystem wrapper for all of Qt.
// Uses private implementation headers of the Qt framework to handle all file
// requests that originate from Qt itself (resource requests, etc).

#include "mainwindow.h"

#include <QtCore/private/qabstractfileengine_p.h>

#include <qdatetime.h>

#include <sdk/GlobPattern.h>

struct FileSystemQtFileEngineIterator final : public QAbstractFileEngineIterator
{
    AINLINE FileSystemQtFileEngineIterator( QDir::Filters filters, const QStringList& nameFilters, QStringList list ) : QAbstractFileEngineIterator( std::move( filters ), nameFilters ), filenames( std::move( list ) )
    {
        return;
    }

    QString next( void ) override
    {
        if ( !hasNext() )
        {
            return QString();
        }

        this->entryIndex++;

        return currentFilePath();
    }

    bool hasNext( void ) const override
    {
        return ( this->entryIndex + 1 < this->filenames.size() );
    }

    QString currentFileName( void ) const override
    {
        if ( this->entryIndex >= this->filenames.size() )
        {
            return QString();
        }

        return this->filenames[ this->entryIndex ];
    }

private:
    size_t entryIndex = 0;
    QStringList filenames;
};

// Translators that are visited in-order for files.
static eir::Vector <CFileTranslator*, FileSysCommonAllocator> translators;

struct FileSystemQtFileEngine final : public QAbstractFileEngine
{
    // Since there is no reason to access files outside of our executable,
    // we simply can look up all important translators from an internal
    // registry. There may be a case when fonts and system stuff must be
    // accessed through strange paths, so be prepared to add-back a
    // translator member.

    inline FileSystemQtFileEngine( const QString& fileName )
    {
        this->location = qt_to_filePath( fileName );
        this->dataFile = nullptr;
    }

    inline void _closeDataFile( void )
    {
        if ( CFile *dataFile = this->dataFile )
        {
            delete dataFile;
            
            this->dataFile = nullptr;
        }
    }

    ~FileSystemQtFileEngine( void )
    {
        this->_closeDataFile();
    }

    inline CFile* open_first_translator_file( const filePath& thePath, const filesysOpenMode& mode )
    {
        size_t transCount = translators.GetCount();

        for ( size_t n = 0; n < transCount; n++ )
        {
            CFileTranslator *trans = translators[ n ];

            CFile *openFile = trans->Open( thePath, mode );

            if ( openFile != nullptr )
            {
                return openFile;
            }
        }

        return nullptr;
    }

    bool open( QIODevice::OpenMode openMode ) override
    {
        // We do not support certain things.
        if ( openMode & QIODevice::OpenModeFlag::Append )
        {
            return false;
        }
        if ( openMode & QIODevice::OpenModeFlag::NewOnly )
        {
            return false;
        }
        // ignore text.
        // ignore unbuffered.

        // Create the file opening mode.
        filesysOpenMode mode;
        mode.access.allowRead = ( openMode & QIODevice::OpenModeFlag::ReadOnly );
        mode.access.allowWrite = ( openMode & QIODevice::OpenModeFlag::WriteOnly );
        mode.seekAtEnd = ( openMode & QIODevice::OpenModeFlag::Append );
        
        eFileOpenDisposition openType;

        if ( openMode & QIODevice::OpenModeFlag::NewOnly )
        {
            openType = eFileOpenDisposition::CREATE_NO_OVERWRITE;
        }
        else if ( openMode & QIODevice::OpenModeFlag::ExistingOnly )
        {
            // TODO: maybe add truncate-if-existing logic.

            openType = eFileOpenDisposition::OPEN_EXISTS;
        }
        else
        {
            if ( openMode & QIODevice::OpenModeFlag::Truncate )
            {
                openType = eFileOpenDisposition::CREATE_OVERWRITE;
            }
            else
            {
                openType = eFileOpenDisposition::OPEN_OR_CREATE;
            }
        }

        CFile *openedFile = open_first_translator_file( this->location, mode );

        if ( openedFile )
        {
            this->_closeDataFile();

            this->dataFile = openedFile;

            return true;
        }
        
        return false;
    }

    bool close( void ) override
    {
        this->_closeDataFile();

        return true;
    }

    bool flush( void ) override
    {
        if ( CFile *dataFile = this->dataFile )
        {
            dataFile->Flush();
            return true;
        }

        return false;
    }

    bool syncToDisk( void ) override
    {
        if ( CFile *dataFile = this->dataFile )
        {
            dataFile->Flush();
            return true;
        }

        return false;
    }

    qint64 size( void ) const override
    {
        if ( CFile *dataFile = this->dataFile )
        {
            return dataFile->GetSizeNative();
        }

        return (qint64)fileRoot->Size( this->location );
    }

    qint64 pos( void ) const override
    {
        if ( CFile *dataFile = this->dataFile )
        {
            return dataFile->TellNative();
        }

        return 0;
    }

    bool seek( qint64 pos ) override
    {
        if ( CFile *dataFile = this->dataFile )
        {
            return dataFile->SeekNative( pos, SEEK_SET );
        }

        return false;
    }

    bool isSequential( void ) const override
    {
        // Assumingly we will only have non-sequential streams here?
        return false;
    }

    bool remove( void ) override
    {
        bool hasRemoved = false;

        size_t numTranslators = translators.GetCount();

        for ( size_t n = 0; n < numTranslators; n++ )
        {
            CFileTranslator *trans = translators[ n ];

            bool couldRemove = trans->Delete( this->location );

            if ( couldRemove )
            {
                hasRemoved = true;
            }
        }

        return hasRemoved;
    }

    bool copy( const QString& newName ) override
    {
        filesysOpenMode readOpen;
        FileSystem::ParseOpenMode( "rb", readOpen );

        FileSystem::filePtr srcFile = open_first_translator_file( this->location, readOpen );

        if ( !srcFile.is_good() )
            return false;

        filesysOpenMode writeOpen;
        FileSystem::ParseOpenMode( "wb", writeOpen );

        FileSystem::filePtr dstFile = open_first_translator_file( qt_to_filePath( newName ), writeOpen );

        if ( !dstFile.is_good() )
            return false;

        FileSystem::StreamCopy( *srcFile, *dstFile );
        return true;
    }

    bool rename( const QString& newName ) override
    {
        size_t transCount = translators.GetCount();

        for ( size_t n = 0; n < transCount; n++ )
        {
            CFileTranslator *trans = translators[ n ];

            if ( trans->Rename( this->location, qt_to_filePath( newName ) ) )
            {
                return true;
            }
        }

        return false;
    }

    bool renameOverwrite( const QString& newName ) override
    {
        // Maybe implement this.
        return false;
    }

    bool link( const QString& newName ) override
    {
        // Maybe implement this.
        return false;
    }

    bool mkdir( const QString& dirName, bool createParentDirectories ) const override
    {
        if ( translators.GetCount() == 0 )
            return false;

        // We always create parent directories; we could implement a switch to disable it.

        CFileTranslator *first = translators[ 0 ];

        return first->CreateDir( qt_to_filePath( dirName ) + "/" );
    }

    bool rmdir( const QString& dirName, bool recurseParentDirectories ) const override
    {
        // Ignore deleting empty parent directories for now; as said above we do things on demand.
        // Also we can delete files using this function; maybe implement a switch in FileSystem?

        bool hasDeleted = false;

        size_t transCount = translators.GetCount();

        filePath fsDirName = ( qt_to_filePath( dirName ) + "/" );

        for ( size_t n = 0; n < transCount; n++ )
        {
            CFileTranslator *trans = translators[ n ];

            if ( trans->Delete( fsDirName ) )
            {
                hasDeleted = true;
            }
        }

        return hasDeleted;
    }

    bool setSize( qint64 size ) override
    {
        if ( CFile *dataFile = this->dataFile )
        {
            dataFile->SeekNative( size, SEEK_SET );
            dataFile->SetSeekEnd();
            return true;
        }

        return false;
    }

    bool caseSensitive( void ) const override
    {
        // Just return what the platform mandates.
        return fileRoot->IsCaseSensitive();
    }

    bool isRelativePath( void ) const override
    {
        filePath desc;

        bool isRootPath = fileSystem->GetSystemRootDescriptor( this->location, desc );

        if ( isRootPath )
        {
            return false;
        }

        // TODO: What about translator root paths?

        return true;
    }

    struct _filePath_comparator
    {
        static inline bool is_less_than( const filePath& left, const filePath& right )
        {
            return left.compare( right, true ) == eir::eCompResult::LEFT_LESS;
        }
    };

    QStringList entryList( QDir::Filters filters, const QStringList& filterNames ) const override
    {
        // First make all GLOB patterns for filename matching.
        eir::PathPatternEnv <wchar_t, FileSysCommonAllocator> patternEnv(
            filters & QDir::Filter::CaseSensitive
        );

        eir::Vector <decltype(patternEnv)::filePattern_t, FileSysCommonAllocator> patterns;

        for ( const QString& patDesc : filterNames )
        {
            rw::rwStaticString <wchar_t> widePatDesc = qt_to_widerw( patDesc );

            auto pattern = patternEnv.CreatePattern( widePatDesc.GetConstString() );

            patterns.AddToBack( std::move( pattern ) );
        }

        size_t numPatterns = patterns.GetCount();

        // Prepare filtering options for the fs scan.
        scanFilteringFlags flags;
        flags.noDirectory = ( filters & QDir::Filter::Dirs ) == false;
        flags.noFile = ( filters & QDir::Filter::Files ) == false;
        // ignore QDir::Filter::Drives
        flags.noJunctionOrLink = ( filters & QDir::Filter::NoSymLinks );
        // ignore QDir::Filter::Readable
        // ignore QDir::Filter::Writable
        // ignore QDir::Filter::Executable
        // ignore QDir::Filter::Modified
        flags.noHidden = ( filters & QDir::Filter::Hidden ) == false;
        flags.noSystem = ( filters & QDir::Filter::System ) == false;
        flags.noPatternOnDirs = ( filters & QDir::Filter::AllDirs );
        flags.noCurrentDirDesc = ( filters & QDir::Filter::NoDot );
        // QDir::Filter::CaseSensitive is passed to the patternEnv.
        flags.noParentDirDesc = ( filters & QDir::Filter::NoDotDot );

        // Create a set of all available files on the translators and then turn it into a string list.

        eir::Set <filePath, FileSysCommonAllocator, _filePath_comparator> fileNameSet;

        size_t transCount = translators.GetCount(); 

        filePath dirPath = ( this->location + "/" );

        for ( size_t n = 0; n < transCount; n++ )
        {
            CFileTranslator *trans = translators[ n ];

            FileSystem::dirIterator dirIter = trans->BeginDirectoryListing( dirPath, "*", flags );
            
            if ( !dirIter.is_good() )
                continue;

            CDirectoryIterator::item_info info;

            while ( dirIter->Next( info ) )
            {
                // Does it match the patterns?
                bool hasPatternConflict = false;

                if ( info.attribs.type == eFilesysItemType::DIRECTORY && flags.noPatternOnDirs )
                {
                    hasPatternConflict = false;
                }
                else
                {
                    for ( size_t n = 0; n < numPatterns; n++ )
                    {
                        const decltype(patternEnv)::filePattern_t& pattern = patterns[ n ];

                        if ( patternEnv.MatchPattern( info.filename, pattern ) == false )
                        {
                            hasPatternConflict = true;
                            break;
                        }
                    }
                }

                if ( hasPatternConflict )
                {
                    continue;
                }

                fileNameSet.Insert( std::move( info.filename ) );
            }
        }

        // Convert our result names into a QStringList.
        QStringList result;

        for ( const filePath& name : fileNameSet )
        {
            QString qtName = filePath_to_qt( name );

            result.append( std::move( qtName ) );
        }

        return result;
    }

    FileFlags fileFlags( FileFlags type ) const override
    {
        filesysStats objStats;

        bool gotStats = false;

        size_t numTrans = translators.GetCount();

        for ( size_t n = 0; n < numTrans; n++ )
        {
            CFileTranslator *trans = translators[ n ];

            if ( trans->QueryStats( this->location, objStats ) )
            {
                gotStats = true;
                break;
            }

            if ( trans->QueryStats( this->location + "/", objStats ) )
            {
                gotStats = true;
                break;
            }
        }

        FileFlags flagsOut = 0;
        
        if ( gotStats )
        {
            flagsOut |= FileFlag::ExistsFlag;

            eFilesysItemType itemType = objStats.attribs.type;

            if ( itemType == eFilesysItemType::FILE )
            {
                flagsOut |= FileFlag::FileType;
            }
            else if ( itemType == eFilesysItemType::DIRECTORY )
            {
                flagsOut |= FileFlag::DirectoryType;
            }

            if ( objStats.attribs.isHidden )
            {
                flagsOut |= FileFlag::HiddenFlag;
            }
            if ( objStats.attribs.isJunctionOrLink )
            {
                flagsOut |= FileFlag::LinkType;
            }
        }

        return flagsOut;
    }

    bool setPermissions( uint perms ) override
    {
        // Not supported because it is an unix-only permission model.
        return false;
    }

    QByteArray id( void ) const override
    {
        // ???
        return QByteArray();
    }

    QString fileName( FileName file = DefaultName ) const override
    {
        if ( file == FileName::DefaultName )
        {
            return filePath_to_qt( this->location );
        }
        if ( file == FileName::BaseName )
        {
            filePath baseName = FileSystem::GetFileNameItem( this->location, true );

            return filePath_to_qt( baseName );
        }
        if ( file == FileName::PathName )
        {
            filePath dirName;

            FileSystem::GetFileNameItem( this->location, false, &dirName );

            return filePath_to_qt( dirName );
        }
        if ( file == FileName::AbsoluteName )
        {
            filePath absPath;
            fileRoot->GetFullPath( this->location, true, absPath );

            return filePath_to_qt( absPath );
        }
        if ( file == FileName::AbsolutePathName )
        {
            filePath absPath;
            fileRoot->GetFullPath( this->location, false, absPath );

            return filePath_to_qt( absPath );
        }

        return filePath_to_qt( this->location );
    }

    uint ownerId( FileOwner owner ) const override
    {
        // Some linux-only bs.
        return 0;
    }

    QString owner( FileOwner owner ) const override
    {
        // Some linux-only bs.
        return "";
    }

    bool setFileTime( const QDateTime& newData, FileTime time ) override
    {
        if ( CFile *dataFile = this->dataFile )
        {
            time_t timeval = newData.toTime_t();

            // TODO: maybe allow setting just one of the times?

            dataFile->SetFileTimes( timeval, timeval, timeval );
            return true;
        }

        return false;
    }

    QDateTime fileTime( FileTime time ) const override
    {
        // TODO: maybe allow returing a specific time instead of just the modification time?
        if ( CFile *dataFile = this->dataFile )
        {
            filesysStats fileStats;

            if ( dataFile->QueryStats( fileStats ) )
            {
                return QDateTime::fromTime_t( fileStats.ctime );
            }
        }

        filesysStats fileStats;

        if ( fileRoot->QueryStats( this->location, fileStats ) )
        {
            return QDateTime::fromTime_t( fileStats.ctime );
        }

        return QDateTime();
    }

    void setFileName( const QString& file ) override
    {
        this->location = qt_to_filePath( file );
    }

    int handle( void ) const override
    {
        // Not supported.
        return -1;
    }

    bool cloneTo( QAbstractFileEngine *target ) override
    {
        if ( FileSystemQtFileEngine *targetEngine = dynamic_cast <FileSystemQtFileEngine*> ( target ) )
        {
            targetEngine->location = this->location;

            // TODO: what about the opened file?

            return true;
        }

        return false;
    }

    QAbstractFileEngineIterator* beginEntryList( QDir::Filters filters, const QStringList& filterNames ) override
    {
        return new FileSystemQtFileEngineIterator( filters, filterNames, this->entryList( filters, filterNames ) );
    }

    QAbstractFileEngineIterator* endEntryList( void ) override
    {
        // TODO: what is this?
        return nullptr;
    }

    qint64 read( char *data, qint64 maxlen ) override
    {
        if ( CFile *dataFile = this->dataFile )
        {
            return (qint64)dataFile->Read( data, (size_t)maxlen );
        }

        return 0;
    }

    qint64 readLine( char *data, qint64 maxlen ) override
    {
        // Do we have to implement this?
        return 0;
    }

    qint64 write( const char *data, qint64 len ) override
    {
        if ( CFile *dataFile = this->dataFile )
        {
            return (qint64)dataFile->Write( data, (size_t)len );
        }

        return 0;
    }
    
    bool extension( Extension extension, const ExtensionOption *option, ExtensionReturn *output ) override
    {
        return false;
    }

    bool supportsExtension( Extension extension ) const override
    {
        return false;
    }

private:
    filePath location;
    CFile *dataFile;
};

void register_file_translator( CFileTranslator *source )
{
    if ( translators.Find( source ) == false )
    {
        translators.AddToBack( source );
    }
}

void unregister_file_translator( CFileTranslator *source )
{
    translators.RemoveByValue( source );
}

struct FileSystemQtFileEngineHandler : public QAbstractFileEngineHandler
{
    QAbstractFileEngine* create( const QString& fileName ) const override
    {
        return new FileSystemQtFileEngine( fileName );
    }
};

static optional_struct_space <FileSystemQtFileEngineHandler> filesys_qt_wrap;

void registerQtFileSystem( void )
{
    filesys_qt_wrap.Construct();
}

void unregisterQtFileSystem( void )
{
    filesys_qt_wrap.Destroy();
}