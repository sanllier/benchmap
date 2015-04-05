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
#include <sstream>
#include <vector>

#pragma warning(disable : 4996)

//--------------------------------------------------------

struct SParams
{
    int procNumber;
    std::vector< float > probabilities;
    int averageSendSize;
    int averageSleepTime;
    float totalTransferedDataKb;

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

    float probsSum = 0.0;

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
        else if ( 0 == strcmp( "avrg-sleep-time", name ) )
        {
            parsedParams.averageSleepTime = node.attribute( "value" ).as_int(0);
        }
        else if ( 0 == strcmp( "total-transfered-data-kb", name ) )
        {
            parsedParams.totalTransferedDataKb = node.attribute( "value" ).as_float(0);
        }
        else if ( 0 == strcmp( "out-file", name ) )
        {
            parsedParams.outFile = node.attribute( "value" ).as_string();
        }
        else if ( 0 == strcmp( "comm-mtx-file", name ) )
        {
            parsedParams.commMtxFile = node.attribute( "value" ).as_string();
        }
        if ( 0 == strcmp( "probabilities", name ) )
        {
            for ( pugi::xml_node probNode = node.child( "prob" ); probNode; probNode = probNode.next_sibling() )
            {
                const float val = probNode.attribute("p").as_float(-1.0f);
                if ( val < 0.0f || val > 1.0f )
                {
                    std::cout << "Probability was dropped\r\n";
                    continue;
                }
                parsedParams.probabilities.push_back(val);        
                probsSum += val;
            }
        }
    }

    if ( parsedParams.procNumber <= 0 || parsedParams.averageSendSize <= 0 || 
         parsedParams.averageSleepTime < 0 || parsedParams.outFile.empty() ||
         parsedParams.totalTransferedDataKb < 0.0f || parsedParams.probabilities.empty() || probsSum != 1.0f )
         throw std::string( "Invalid configuration. " ).append( __FUNCTION__ );

    fclose(fp);
    delete[] buf;
    return parsedParams;
}

//--------------------------------------------------------

void saveCommMtx( long long** commMtx, int size, const char* fileName )
{
    std::stringstream outData;
    int lines = 0;

    for ( int i = 0; i < size; ++i )
    {
        for ( int q = 0; q < size; ++q )
        {
            if ( commMtx[i][q] != 0 )
            {
                outData << i << " " << q << " " << commMtx[i][q] << "\n";
                ++lines;
            }
         }
    }

    FILE* fp = fopen( fileName, "wb" );
    if ( !fp ) 
        throw std::string( "Problems with comm mtx file. " ).append( __FUNCTION__ );

    std::stringstream desc;
    desc << size << " " << size << " " << lines << "\n";

    size_t res = fwrite( desc.str().c_str(), 1, desc.str().length(), fp );
    if ( res != desc.str().length() )
        throw std::string( "Error while comm mtx writing. " ).append( __FUNCTION__ );

    res = fwrite( outData.str().c_str(), 1, outData.str().length(), fp );
    if ( res != outData.str().length() )
        throw std::string( "Error while comm mtx writing. " ).append( __FUNCTION__ );

    fclose( fp );
}

//--------------------------------------------------------

int getPartition( const std::vector<float>& probs )
{
    float rVal = float(rand()) / RAND_MAX;

    float pSum = 0.0f;
    int part = 0;
    for ( int i = 0; i < probs.size(); ++i )
    {
        pSum += probs[i];
        if ( pSum >= rVal )
            break;

        ++part;
    }

    return part;
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
        long long curProgress = 0;

        long long** commMtx = new long long*[ params.procNumber ];
        for ( int i = 0; i < params.procNumber; ++i )
        {
            commMtx[i] = new long long[ params.procNumber ];
            memset( commMtx[i], 0, params.procNumber * sizeof(**commMtx) );
        }

        std::stringstream trace;
        const int pSize = params.procNumber / params.probabilities.size();

        while ( currentTransferedData < targetTransferedData )
        {
            const int fromPartition = getPartition( params.probabilities );
            const int toPartition   = getPartition( params.probabilities );

            const int from = rand() % pSize;
            const int to   = rand() % pSize;
            if ( from == to && fromPartition == toPartition )
                continue;
                
            const int fromIdx = fromPartition * pSize + from;
            const int toIdx   = toPartition * pSize + to;
            commMtx[ fromIdx ][ toIdx ] += params.averageSendSize;
            commMtx[ toIdx ][ fromIdx ] += params.averageSendSize;

            trace << "s " << fromIdx << " " << toIdx << " " << params.averageSendSize << "\n";
            currentTransferedData += params.averageSendSize;

            if ( currentTransferedData / 1024 > curProgress )
            {
                std::cout << currentTransferedData / 1024 << "/" << params.totalTransferedDataKb << "\n";
                curProgress = currentTransferedData / 1024;
            }
        }

        std::stringstream comments;
        comments << "#transfered: " << currentTransferedData << "\n";
        comments << "%procs_num: " << params.procNumber << "\n";
        comments << "%transfer_buf: " << params.averageSendSize << "\n";
        comments << "%sleep: " << params.averageSleepTime << "\n";
        comments << "-------------------------\n";

        FILE* fp = fopen( params.outFile.c_str(), "wb" );
        if ( !fp ) 
            throw std::string( "Problems with out file. " ).append( __FUNCTION__ );

        size_t res = fwrite( comments.str().c_str(), 1, comments.str().length(), fp );
        if ( res != comments.str().length() )
            throw std::string( "Error while out file writing. " ).append( __FUNCTION__ );

        res = fwrite( trace.str().c_str(), 1, trace.str().length(), fp );
        if ( res != trace.str().length() )
            throw std::string( "Error while out file writing. " ).append( __FUNCTION__ );

        if ( !params.commMtxFile.empty() )
            saveCommMtx( commMtx, params.procNumber, params.commMtxFile.c_str() );

        fclose(fp);
        for ( int i = 0; i < params.procNumber; ++i )
            delete[] commMtx[i];

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
