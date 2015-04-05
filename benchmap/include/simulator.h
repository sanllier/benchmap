#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "parparser.h"
#include <string>
#include <sstream>
#include <time.h>
#include <stdlib.h>

#ifdef _MSC_VER
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#else
    #include <unistd.h>
#endif

//--------------------------------------------------------

int simulator_routine( parparser& args )
{
    try
    {
        int rank = 0;
        int commSize = 0;
        MPI_Comm_rank( MPI_COMM_WORLD, &rank );
        MPI_Comm_size( MPI_COMM_WORLD, &commSize );
    
        srand( unsigned( time(0) ) );

        const char* traceFile = args.get( "t" ).asString(0);
        if ( !traceFile || !traceFile[0] )
            throw std::string( "Invalid trace file name. " ).append( __FUNCTION__ );   

        MPI_File fp = MPI_FILE_NULL;
        MPI_File_open( MPI_COMM_WORLD, const_cast<char*>(traceFile), MPI_MODE_RDONLY, MPI_INFO_NULL, &fp );
    
        MPI_Offset fileSize = 0;
        MPI_File_get_size( fp, &fileSize );
    
        std::string trace;
        trace.resize( unsigned(fileSize) );

        MPI_Status status;    
        MPI_File_read_at_all( fp, 0, &trace[0], int( fileSize ), MPI_CHAR, &status );
        MPI_File_close( &fp );

        std::stringstream sStr( trace );
        std::string line;
        line.resize(1024);
        char symb;

        int bufSize = 0;
        int procsNum = 0;
        int sleepTime = 0;

        do
        {                
            sStr >> symb;
            sStr.getline( &line[0], 1024 );

            if ( symb == '#' ) 
                continue;
            else if ( symb == '%' )
            {
                if ( line.find("transfer_buf") != std::string::npos )
                    bufSize = strtol( &line[ strlen("transfer_buf") + 2 ], 0, 10 );
                else if ( line.find("procs_num") != std::string::npos )
                    procsNum = strtol( &line[ strlen("procs_num") + 2 ], 0, 10 );
                else if ( line.find("sleep") != std::string::npos )
                    sleepTime = strtol( &line[ strlen("sleep") + 2 ], 0, 10 );
            }
        } while (symb != '-');

        if ( commSize < procsNum )
            throw std::string( "Too small communicator. " ).append( __FUNCTION__ );

        if ( rank >= procsNum )
            return 0;

        int from = 0;
        int to = 0;
        int dataSize = 0;
        char* buf = new char[ bufSize ];
        bool needSleep = false;

        double startTime = MPI_Wtime();

        int lineNum = 0;

        while ( !sStr.eof() )
        {
            sStr >> symb;

            if ( lineNum % 500 == 0 && rank == 0 )
            {
                std::cout << lineNum << "\r\n";
                std::cout.flush();
            }
        
            ++lineNum;

            if ( symb == 's' )
            {
                sStr >> from;
                sStr >> to;
                sStr >> dataSize;

                if ( rank == from )
                {
                    MPI_Send( buf, dataSize, MPI_CHAR, to, from, MPI_COMM_WORLD );
                    needSleep = true;
                }
                else if ( rank == to )
                {              
                    MPI_Recv( buf, dataSize, MPI_CHAR, from, from, MPI_COMM_WORLD, &status ); 
                    needSleep = true;
                }

                if ( needSleep )
                {
                    needSleep = false;
                    if ( sleepTime > 0 )
                    #ifdef _MSC_VER
                         Sleep( sleepTime );
                    #else
                        usleep( sleepTime * 1000 );
                    #endif
                }
            } 
            else if ( symb == '#' )
            {
                sStr.getline( &line[0], 1024 );
                continue;
            }
        }

        double totalTime = MPI_Wtime() - startTime;

        if ( rank == 0 )
            std::cout << totalTime;

        delete[] buf;
    }
    catch( std::string err )
    {        
        std::cerr << "ERROR OCCURED:\n    " << err << "\n";
        std::cerr.flush();
    }

    return 0;
}

//--------------------------------------------------------
#endif
