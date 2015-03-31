#include <iostream>
#include "generator.h"
#include "simulator.h"
#include "parparser.h"
#include "mpi.h"

//--------------------------------------------------------

int main( int argc, char** argv )
{
    MPI_Init( &argc, &argv );

    parparser parameters( argc, argv );
    bool generate = parameters.get( "g" ).asBool( false );

    int retCode = generate ? generator_routine( parameters ) : simulator_routine( parameters );

    MPI_Finalize();
    return retCode;
}

//--------------------------------------------------------
