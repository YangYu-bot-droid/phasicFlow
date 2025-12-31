/*------------------------------- phasicFlow ---------------------------------
      O        C enter of
     O O       E ngineering and
    O   O      M ultiscale modeling of
   OOOOOOO     F luid flow       
------------------------------------------------------------------------------
  Copyright (C): www.cemf.ir
  email: hamid.r.norouzi AT gmail.com
------------------------------------------------------------------------------  
Licence:
  This file is part of phasicFlow code. It is a free software for simulating 
  granular and multiphase flows. You can redistribute it and/or modify it under
  the terms of GNU General Public License v3 or any other later versions. 
 
  phasicFlow is distributed to help others in their research in the field of 
  granular and multiphase flows, but WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

-----------------------------------------------------------------------------*/

#ifndef __TavaresBreakageEvent_hpp__
#define __TavaresBreakageEvent_hpp__

#include "types.hpp"

namespace pFlow
{

/**
 * @brief Breakage event data structure
 * 
 * Stores information about a particle breakage event that occurred
 * during contact force calculations. Events are collected and processed
 * separately to handle particle insertion/removal.
 */
struct TavaresBreakageEvent
{
	uint32  particleId;           // ID of particle that broke
	real    diameter;             // Diameter of parent particle
	real    impactEnergy;         // Impact energy at breakage
	realx3  position;             // Position of breakage
	realx3  velocity;             // Velocity at breakage
	uint32  propertyId;           // Material property ID
	real    breakageProbability;  // Calculated breakage probability
	
	INLINE_FUNCTION_HD
	TavaresBreakageEvent()
	:
		particleId(0),
		diameter(0.0),
		impactEnergy(0.0),
		position(0.0),
		velocity(0.0),
		propertyId(0),
		breakageProbability(0.0)
	{}
	
	INLINE_FUNCTION_HD
	TavaresBreakageEvent(
		uint32 id,
		real diam,
		real energy,
		const realx3& pos,
		const realx3& vel,
		uint32 propId,
		real prob
	)
	:
		particleId(id),
		diameter(diam),
		impactEnergy(energy),
		position(pos),
		velocity(vel),
		propertyId(propId),
		breakageProbability(prob)
	{}
};

/**
 * @brief Fragment generation parameters
 * 
 * Defines how to generate daughter particles from a parent particle
 * based on the Tavares breakage model
 */
struct TavaresFragmentParams
{
	uint32  numFragments;         // Number of daughter particles
	real    sizeRatio;            // Size ratio for fragments (d_frag / d_parent)
	real    energyFraction;       // Fraction of kinetic energy retained
	
	// Rosin-Rammler distribution parameters
	real    rr_x0;                // Characteristic size
	real    rr_n;                 // Uniformity parameter
	
	INLINE_FUNCTION_HD
	TavaresFragmentParams()
	:
		numFragments(2),
		sizeRatio(0.7),
		energyFraction(0.5),
		rr_x0(0.5),
		rr_n(1.0)
	{}
	
	/**
	 * @brief Calculate fragment size using Rosin-Rammler distribution
	 * @param parentDiameter Diameter of parent particle
	 * @param fragmentIndex Index of fragment (0 to numFragments-1)
	 * @return Diameter of fragment particle
	 */
	INLINE_FUNCTION_HD
	real fragmentSize(real parentDiameter, uint32 fragmentIndex) const
	{
		// Simple power law distribution for now
		// In full implementation, use proper Rosin-Rammler or other distributions
		real baseFraction = pow(sizeRatio, static_cast<real>(1.0) / numFragments);
		real sizeFactor = pow(baseFraction, static_cast<real>(fragmentIndex + 1));
		return parentDiameter * sizeFactor;
	}
	
	/**
	 * @brief Calculate total volume of fragments to check mass conservation
	 * @param parentDiameter Diameter of parent particle
	 * @return Total volume of all fragments
	 */
	INLINE_FUNCTION_HD
	real totalFragmentVolume(real parentDiameter) const
	{
		real totalVol = 0.0;
		for(uint32 i = 0; i < numFragments; ++i)
		{
			real d_frag = fragmentSize(parentDiameter, i);
			real r_frag = d_frag / 2.0;
			totalVol += 4.0/3.0 * Pi * r_frag * r_frag * r_frag;
		}
		return totalVol;
	}
	
	/**
	 * @brief Normalize fragment sizes to conserve mass
	 * @param parentDiameter Diameter of parent particle
	 * @param fragmentIndex Index of fragment
	 * @return Mass-conserving fragment diameter
	 */
	INLINE_FUNCTION_HD
	real normalizedFragmentSize(real parentDiameter, uint32 fragmentIndex) const
	{
		real parentVolume = 4.0/3.0 * Pi * pow(parentDiameter/2.0, 3.0);
		real totalFragVolume = totalFragmentVolume(parentDiameter);
		real scaleFactor = pow(parentVolume / totalFragVolume, 1.0/3.0);
		return fragmentSize(parentDiameter, fragmentIndex) * scaleFactor;
	}
};

} // namespace pFlow

#endif // __TavaresBreakageEvent_hpp__
