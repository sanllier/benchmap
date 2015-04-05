#ifndef MATRIX_MPI_H
#define MATRIX_MPI_H

#include <map>
#include <string>

//--------------------------------------------------------------

class KVPair
{
public:
    KVPair( const char* key = 0, const char* value = 0 );
    ~KVPair();

    bool asBool( bool def = false );
    int asInt( int def = 0 );
    long asLong( long def = 0 );
    float asFloat( float def = 0.0f );
    double asDouble( double def = 0.0 );
    const char* asString( const char* def = "" );

private:
    void set( const char* key, const char* value );

private:
    const char* m_key;
    const char* m_value;
    bool m_isSet;
};

//--------------------------------------------------------------

class parparser
{
public:
    parparser() {}
    parparser( int argc, char** argv );
    ~parparser() {}

    bool parse( int argc, char** argv );
    void clear() { m_map.clear(); }

    KVPair get( const char* key );

private:
    std::map< std::string, std::string > m_map;
};

#endif
