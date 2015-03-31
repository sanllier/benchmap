#ifndef GENERATOR_H
#define GENERATOR_H

#include "parparser.h"
#include "mpi.h"
#include "pugixml.hpp"

#include <string>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#pragma warning(disable : 4996)

//--------------------------------------------------------

struct SParams
{
    int procNumber;
    int averageSendSize;
    int averageSendSizeDispersion;
    int averageSleepTime;
    int averageSleepTimeDispersion;
    unsigned totalTransferedDataKb;
    int neighborsNumber;
    std::string outFile;
    std::string commMtxFile;
};

//--------------------------------------------------------

SParams readXMLConfig( const char* fileName )
{
    if ( !fileName || !fileName[0] )
        throw std::string( "Invalid config file name. " ).append( __FUNCTION__ );

    FILE* fp = fopen( fileName, "rb" );
    if ( !fp ) 
        throw std::string( "Invalid config file. " ).append( __FUNCTION__ );

    fseek (fp , 0 , SEEK_END);
    long fileSize = ftell(fp);
    rewind(fp);

    char* buf = new char[ fileSize ];
    size_t res = fread( buf, 1, fileSize, fp);
    if ( res != fileSize )
        throw std::string( "Error while config file reading. " ).append( __FUNCTION__ );

    pugi::xml_document doc;
    doc.load_buffer( &buf[0], unsigned( fileSize ) );
    
    pugi::xml_node rootNode = doc.child( "benchmap" );    
    if ( !rootNode )
        throw std::string( "Some problems with config file. " ).append( __FUNCTION__ );

    SParams parsedParams;

    for ( pugi::xml_node node = rootNode.child( "parameter" ); node; node = node.next_sibling() )
    {
        const char* name = node.attribute( "name" ).as_string(0);
        if ( !name )
            continue;

        if ( 0 == strcmp( "processors-number", name ) )
        {
            parsedParams.procNumber = node.attribute( "value" ).as_int(0);
        }
        else if ( 0 == strcmp( "avrg-send-size", name ) )
        {
            parsedParams.averageSendSize = node.attribute( "value" ).as_int(0);
        }
        else if ( 0 == strcmp( "avrg-send-size-dispersion", name ) )
        {
            parsedParams.averageSendSizeDispersion = node.attribute( "value" ).as_int(0);
        }
        else if ( 0 == strcmp( "avrg-sleep-time", name ) )
        {
            parsedParams.averageSleepTime = node.attribute( "value" ).as_int(0);
        }
        else if ( 0 == strcmp( "avrg-sleep-time-dispersion", name ) )
        {
            parsedParams.averageSleepTimeDispersion = node.attribute( "value" ).as_int(0);
        }
        else if ( 0 == strcmp( "total-transfered-data-kb", name ) )
        {
            parsedParams.totalTransferedDataKb = node.attribute( "value" ).as_uint(0);
        }
        else if ( 0 == strcmp( "neighbors-number", name ) )
        {
            parsedParams.neighborsNumber = node.attribute( "value" ).as_int(0);
        }
        else if ( 0 == strcmp( "out-file", name ) )
        {
            parsedParams.outFile = node.attribute( "value" ).as_string();
        }
        else if ( 0 == strcmp( "comm-mtx-file", name ) )
        {
            parsedParams.commMtxFile = node.attribute( "value" ).as_string();
        }
    }

    if ( parsedParams.procNumber <= 0 || parsedParams.averageSendSize <= 0 || 
         parsedParams.averageSendSizeDispersion < 0 || parsedParams.averageSleepTime < 0 || 
         parsedParams.averageSleepTimeDispersion < 0 || parsedParams.outFile.empty() ||
         parsedParams.totalTransferedDataKb < 0 || parsedParams.neighborsNumber < 0)
         throw std::string( "Invalid configuration. " ).append( __FUNCTION__ );

    fclose(fp);
    delete[] buf;
    return parsedParams;
}

//--------------------------------------------------------

void saveCommMtx( long long** commMtx, int size, const char* fileName )
{
    std::string outData;
    int lines = 0;

    for ( int i = 0; i < size; ++i )
    {
        for ( int q = 0; q < size; ++q )
        {
            if ( commMtx[i][q] != 0 )
            {
                outData.append( std::to_string(i) )
                       .append(" ")
                       .append( std::to_string(q) )
                       .append(" ")
                       .append( std::to_string( commMtx[i][q] ) )
                       .append("\n");
                ++lines;
            }
         }
    }

    FILE* fp = fopen( fileName, "wb" );
    if ( !fp ) 
        throw std::string( "Problems with comm mtx file. " ).append( __FUNCTION__ );

    std::string desc;
    desc.append( std::to_string(size) )
        .append(" ")
        .append( std::to_string(size) )
        .append(" ")
        .append( std::to_string(lines) )
        .append("\n");

    size_t res = fwrite( desc.c_str(), 1, desc.length(), fp );
    if ( res != desc.length() )
        throw std::string( "Error while comm mtx writing. " ).append( __FUNCTION__ );

    res = fwrite( outData.c_str(), 1, outData.length(), fp );
    if ( res != outData.length() )
        throw std::string( "Error while comm mtx writing. " ).append( __FUNCTION__ );

    fclose( fp );
}

//--------------------------------------------------------

int generator_routine( parparser& args )
{
    int rank = 0;
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    if ( rank != 0 )
        return 0;

    srand( unsigned(time(0)) );

    try
    {
        const char* configFile = args.get( "xml" ).asString(0);
        SParams params = readXMLConfig( configFile );

        const long long targetTransferedData = params.totalTransferedDataKb * 1024;
        long long currentTransferedData = 0;
        long long curProcess = 0;
        int* neighborsNum = new int[ params.procNumber ];
        memset( neighborsNum, 0, sizeof( *neighborsNum ) * params.procNumber );

        bool** links = new bool*[ params.procNumber ];
        for ( int i = 0; i < params.procNumber; ++i )
        {
            links[i] = new bool[ params.procNumber ];
            memset( links[i], 0, params.procNumber );
        }

        long long** commMtx = new long long*[ params.procNumber ];
        for ( int i = 0; i < params.procNumber; ++i )
        {
            commMtx[i] = new long long[ params.procNumber ];
            memset( commMtx[i], 0, params.procNumber * sizeof(**commMtx) );
        }

        int biggestTransfer = 0;

        std::string trace;

        while ( currentTransferedData < targetTransferedData )
        {
            int from = rand() % params.procNumber;
            int to   = rand() % params.procNumber;
            if ( from == to )
                to = to + 1 >= params.procNumber ? to - 1 : to + 1;
            
            if ( params.neighborsNumber > 0 )
            {
                const int storeFrom = from;
                const int storeTo   = to;

                while ( from < params.procNumber && neighborsNum[ from ] >= params.neighborsNumber ) ++from;
                while ( to < params.procNumber && neighborsNum[ to ] >= params.neighborsNumber ) ++to;

                if ( from >= params.procNumber )
                {
                    from = 0;
                    while ( from < params.procNumber && from < storeFrom && neighborsNum[ from ] >= params.neighborsNumber ) ++from;
                }
                if ( to >= params.procNumber )
                {
                    to = 0;
                    while ( to < params.procNumber && to < storeTo && neighborsNum[ to ] >= params.neighborsNumber ) ++to;
                }

                if ( from >= params.procNumber || to >= params.procNumber || from == to ) 
                {
                    from = rand() % params.procNumber;
                    to   = from == 0 ? 1 : 0;
                    while ( to < params.procNumber && !links[from][to] ) ++to;

                    if ( to >= params.procNumber )
                        continue;
                }
            }

            const int transferDisp = rand() % params.averageSendSizeDispersion;
            const int transferSize = params.averageSendSize + transferDisp - ( params.averageSendSizeDispersion / 2 );
            
            if ( transferSize > biggestTransfer )
                biggestTransfer = transferSize;

            if ( !links[from][to] )
            {
                ++neighborsNum[from];
                ++neighborsNum[to];
                links[from][to] = true;
            }

            commMtx[from][to] += transferSize;

            trace.append("s ")
                    .append( std::to_string(from) )
                    .append(" ")
                    .append( std::to_string(to) )
                    .append(" ")
                    .append( std::to_string( transferSize ) )
                    .append("\n");

            currentTransferedData += transferSize;

            if ( currentTransferedData / 1024 > curProcess )
            {
                std::cout << currentTransferedData / 1024 << "/" << params.totalTransferedDataKb << "\n";
                curProcess = currentTransferedData / 1024;
            }
        }

        int avrgNNum = 0;
        for ( int i = 0; i < params.procNumber; ++i )
            avrgNNum += neighborsNum[i];
        avrgNNum /= params.procNumber;

        std::string comments;
        comments.append("#transfered: ").append( std::to_string( currentTransferedData ) ).append("\n");
        comments.append("#avrg_neighbors_num: ").append( std::to_string( avrgNNum ) ).append("\n");
        comments.append("%procs_num: ").append( std::to_string( params.procNumber ) ).append("\n");
        comments.append("%transfer_buf: ").append( std::to_string( biggestTransfer ) ).append("\n");
        comments.append("%sleep: ").append( std::to_string( params.averageSleepTime ) ).append("\n");
        comments.append("%sleep_disp: ").append( std::to_string( params.averageSleepTimeDispersion ) ).append("\n");
        comments.append("-------------------------\n");

        FILE* fp = fopen( params.outFile.c_str(), "wb" );
        if ( !fp ) 
            throw std::string( "Problems with out file. " ).append( __FUNCTION__ );

        size_t res = fwrite( comments.c_str(), 1, comments.length(), fp );
        if ( res != comments.length() )
            throw std::string( "Error while out file writing. " ).append( __FUNCTION__ );

        res = fwrite( trace.c_str(), 1, trace.length(), fp );
        if ( res != trace.length() )
            throw std::string( "Error while out file writing. " ).append( __FUNCTION__ );

        if ( !params.commMtxFile.empty() )
            saveCommMtx( commMtx, params.procNumber, params.commMtxFile.c_str() );

        fclose(fp);
        delete[] neighborsNum;
        for ( int i = 0; i < params.procNumber; ++i )
        {
            delete[] links[i];
            delete[] commMtx[i];
        }
        delete[] links;
        delete[] commMtx;
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
