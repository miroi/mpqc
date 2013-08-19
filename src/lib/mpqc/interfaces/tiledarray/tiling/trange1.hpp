//
// trange1.hpp
//
// Copyright (C) 2013 Drew Lewis
//
// Authors: Drew Lewis
// Maintainer: Drew Lewis and Edward Valeev
//
// This file is part of the MPQC Toolkit.
//
// The MPQC Toolkit is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// The MPQC Toolkit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the MPQC Toolkit; see the file COPYING.LIB.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
// The U.S. Government is granted a limited license as per AL 91-7.
//

#ifndef mpqc_interface_tiledarray_trange1_hpp
#define mpqc_interface_tiledarray_trange1_hpp

#include <tiled_array.h>
#include <chemistry/qc/basis/basis.h>
#include <vector>

/**
 * @defgroup TaTiling mpqc.TiledArrayInterfaces.Tiling
 * @ingroup TiledArrayInterface
 * Tiling information and functions for the MPQC-TiledArray interface
 *
 */

namespace mpqc {
namespace tiling {
    /// @addtogroup TaTiling
    /// @{

    using RefBasis = sc::Ref<sc::GaussianBasisSet>;
    using TRange1 = TiledArray::TiledRange1;
#ifndef DOXYGEN
    // Initialize the vector needed for TiledArray::TiledRange1 construction
    std::vector<std::size_t> vec_init(std::size_t size_guess){
        std::vector<std::size_t> vec_guess;
        vec_guess.reserve(size_guess); //  Reserve space
        vec_guess.push_back(0); // Tile 0 always starts at 0
        return vec_guess;
    }
#endif // DOXYGEN

    /**
     * Returns TiledArray::TiledRange1 that corresponds to integral shells.
     * @param[in] basis Is a sc::GaussiangBasisSet
     */
    TRange1 tile_by_shell(const RefBasis &basis){

        // Shell and basis set information
        std::size_t nshell = basis->nshell();
        std::size_t nbasis = basis->nbasis();

        std::vector<std::size_t> tilesizes = vec_init(nshell);


        // If we have some shells
        if(nshell != 0){
            // Loop over shells
            for(std::size_t i = 0; i < nshell; ++i){
                // Compute one past the last funciton in the shell.
                // If i is the last shell then one past the last function
                // on the shell is one past the last function in the basis.
                std::size_t shell_end = (i == nshell - 1) ? nbasis :
                                basis->shell_to_function(i + 1);

                // Push back the last function on the shell.
                // This way if shell one has 1 function and shell two has 3
                // functions . The vector will look like vector[0] = |0,1) and
                // vector[1] = |1,4)
                tilesizes.push_back(shell_end);
            }
        }
        else {
            std::cout << "ADD SOME EXCEPTION STUFF HERE PRONTO" << std::endl;
        }

        // construct TiledRange1
        return TRange1(tilesizes.begin(), tilesizes.end());
    }

    /**
     * Returns a TiledArray::TiledRange1 where each block
     * corresponds to all the shells on a single atom.
     * @param[in] basis Is a sc::GaussiangBasisSet
     */
    TRange1 tile_by_atom(const RefBasis &basis){

        // Basis set information
        std::size_t ncenters = basis->ncenter();
        std::vector<std::size_t> tilesizes = vec_init(ncenters);

        // If we have atoms
        if(ncenters != 0){
            // Loop over atoms
            for(std::size_t i = 0; i < ncenters; ++i){
                // For each atom add the number of functions to the
                // total number of functions so far.
                tilesizes.push_back(tilesizes[i] +
                                    basis->nbasis_on_center(i)
                                    );
            }
        }
        else {
            std::cout << "ADD SOME EXCEPTION STUFF HERE PRONTO" << std::endl;
        }

        // construct TiledRange1
        return TRange1(tilesizes.begin(), tilesizes.end());
    }

#if 0
    /**
     * creates a TiledArray::TiledRange1 that corresponds to
     * atoms. With the exception that hydrogen gets added to the next heavy
     * atom in the molecule. This is the default tiling stucture.
     */
    TRange1 by_heavy_atom(const RefBasis &basis){

        // Get number of atoms.
        std::size_t ncenters = basis->ncenter();
        std::vector<std::size_t> tilesizes = vec_init(ncenter);

        // if we have atoms
        if(ncneters != 0){
            // Loop over atoms
            for(auto i = 0; i < ncneters; ++i){
                // Check if Hydrogen
                std::size_t Z = basis->molecule()->Z(i);
                if(Z != 1){
                    tilesizes.push_back(
                            tilesizes[i] + basis->nbasis_on_center(i)
                            );
                }
                else {

                }
            }
        }



    }
#endif

    /// @} // TaTiling
} // namespace tiling

    typedef tiling::TRange1 (*TRange1Gen)(const tiling::RefBasis &);

} // namespace mpqc



#endif /* mpqc_interface_tiledarray_trange1_hpp */