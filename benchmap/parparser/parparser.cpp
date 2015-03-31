#include "parparser.h"

#include <string.h>
#include <stdlib.h>

//--------------------------------------------------------------

KVPair::KVPair( const char* key/* = 0*/, const char* value/* = 0*/ )
    : m_key(0)
    , m_value(0)
    , m_isSet( false ) 
{
    if ( key && value )
        set( key, value );
}
KVPair::~KVPair()
{

}

bool KVPair::asBool( bool def/* = false*/ ) 
{ 
    return m_isSet ? ( 0 == strcmp( "t", m_value ) || 0 == strcmp( "true", m_value ) ) : def; 
}

int KVPair::asInt( int def/* = 0*/ ) 
{ 
    return m_isSet ? atoi( m_value ) : def; 
}

long KVPair::asLong( long def/* = 0*/ ) 
{ 
    return m_isSet ? atol( m_value ) : def; 
}

float KVPair::asFloat( float def/* = 0.0f*/ ) 
{ 
    return m_isSet ? (float)atof( m_value ) : def; 
}

double KVPair::asDouble( double def/* = 0.0*/ ) 
{ 
    return m_isSet ? atof( m_value ) : def; 
}

const char* KVPair::asString( const char* def/* = ""*/ ) 
{ 
    return m_isSet ? m_value : def; 
}

void KVPair::set( const char* key, const char* value ) 
{ 
    this->m_key = key; 
    this->m_value = value; 
    m_isSet = true; 
}

//--------------------------------------------------------------

parparser::parparser( int argc, char** argv )
{
    if ( argv && !parse( argc, argv ) )
        clear();
}

bool parparser::parse( int argc, char** argv )
{
    for ( int i = 1; i < argc; ++i )
    {
        if ( argv[i][0] == '-' )
        {
            if ( argv[i][1] == '-' )
            {
                const char* delimPos = strstr( argv[i], "=" );
                if ( !delimPos )
                    continue;

                std::string key = argv[i] + 2;
                key.resize( delimPos - argv[i] - 2 );
                std::string value = argv[i] - delimPos == 0 ? "" : delimPos + 1;
                
                m_map[ key ] = value;
            }
            else
            {
                if ( i + 1 >= argc )
                    return false;

                std::string key = argv[i] + 1;
                std::string value = argv[ i + 1 ];
                if ( value[0] == '-' )
                    return false;

                ++i;

                m_map[ key ] = value;
            }
        }
    }

    return true;
}

KVPair parparser::get( const char* key )
{
    std::map< std::string, std::string >::iterator found = m_map.find( key );
    if ( found == m_map.end() )
        return KVPair();
    else
        return KVPair( found->first.c_str(), found->second.c_str() );
}
