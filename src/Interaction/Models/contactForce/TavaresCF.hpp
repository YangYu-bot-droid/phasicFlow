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

#ifndef __TavaresCF_hpp__
#define __TavaresCF_hpp__

#include "types.hpp"
#include "symArrays.hpp"

namespace pFlow::cfModels
{

/**
 * @brief Tavares breakage model for particle fragmentation
 * 
 * This model implements the Tavares and King (1998) particle breakage model
 * which predicts particle fragmentation based on impact energy.
 * The breakage probability is calculated based on:
 * P(E) = 1 - exp(-(E/E50)^gamma)
 * where E is the specific impact energy, E50 is the characteristic energy
 * for 50% breakage probability, and gamma is the breakage rate parameter.
 * 
 * @tparam limited Whether to apply friction limiting
 */
template<bool limited=true>
class Tavares
{
public:

	struct contactForceStorage
	{
		realx3 overlap_t_ = 0.0;
		real   accumulated_energy_ = 0.0;  // Accumulated impact energy
		bool   breakage_occurred_ = false;  // Flag for breakage detection
	};

	struct TavaresProperties
	{		
		real 	kn_ 	= 1000.0;     // Normal stiffness
		real 	kt_		= 800.0;      // Tangential stiffness
		real 	ethan_ 	= 0.0;        // Normal damping coefficient
		real 	ethat_ 	= 0.0;        // Tangential damping coefficient
		real    mu_ 	= 0.00001;    // Friction coefficient
		real    E50_    = 1.0e-3;     // Characteristic energy (J/kg)
		real    gamma_  = 1.0;        // Breakage rate parameter
		real    min_breakage_size_ = 1.0e-5;  // Minimum size for breakage (m)

		INLINE_FUNCTION_HD
		TavaresProperties(){}

		INLINE_FUNCTION_HD
		TavaresProperties(real kn, real kt, real etha_n, real etha_t, real mu,
		                  real E50, real gamma, real min_size):
			kn_(kn), kt_(kt), ethan_(etha_n), ethat_(etha_t), mu_(mu),
			E50_(E50), gamma_(gamma), min_breakage_size_(min_size)
		{}		

		INLINE_FUNCTION_HD
		TavaresProperties(const TavaresProperties&)=default;

		INLINE_FUNCTION_HD
		TavaresProperties& operator=(const TavaresProperties&)=default;

		INLINE_FUNCTION_HD
		~TavaresProperties() = default;
	};

protected:

	using TavaresArrayType = symArray<TavaresProperties>;

	int32 					numMaterial_ = 0;

	ViewType1D<real>  		rho_;

	TavaresArrayType  		TavaresProperties_;

	/**/

	bool readTavaresDictionary(const dictionary& dict)
	{
		auto kn = dict.getVal<realVector>("kn");
		auto kt = dict.getVal<realVector>("kt");
		auto en = dict.getVal<realVector>("en");
		auto et = dict.getVal<realVector>("et");
		auto mu = dict.getVal<realVector>("mu");
		
		// Tavares-specific parameters
		auto E50 = dict.getValOrSet<realVector>("E50", realVector(kn.size(), 1.0e-3));
		auto gamma = dict.getValOrSet<realVector>("gamma", realVector(kn.size(), 1.0));
		auto minBreakageSize = dict.getValOrSet<realVector>("minBreakageSize", 
		                                                      realVector(kn.size(), 1.0e-5));

		auto nElem = kn.size();

		if(nElem != kt.size())
		{
			fatalErrorInFunction<<
			"sizes of kn("<<nElem<<") and kt("<<kt.size()<<") do not match.\n";
			return false;
		}

		if(nElem != en.size())
		{
			fatalErrorInFunction<<
			"sizes of kn("<<nElem<<") and en("<<en.size()<<") do not match.\n";
			return false;
		}

		if(nElem != et.size())
		{
			fatalErrorInFunction<<
			"sizes of kn("<<nElem<<") and et("<<et.size()<<") do not match.\n";
			return false;
		}

		if(nElem != mu.size())
		{
			fatalErrorInFunction<<
			"sizes of kn("<<nElem<<") and mu("<<mu.size()<<") do not match.\n";
			return false;
		}

		if(nElem != E50.size())
		{
			fatalErrorInFunction<<
			"sizes of kn("<<nElem<<") and E50("<<E50.size()<<") do not match.\n";
			return false;
		}

		if(nElem != gamma.size())
		{
			fatalErrorInFunction<<
			"sizes of kn("<<nElem<<") and gamma("<<gamma.size()<<") do not match.\n";
			return false;
		}

		if(nElem != minBreakageSize.size())
		{
			fatalErrorInFunction<<
			"sizes of kn("<<nElem<<") and minBreakageSize("<<minBreakageSize.size()<<") do not match.\n";
			return false;
		}
		
		// check if size of vector matchs a symetric array
		uint32 nMat;
		if( !TavaresArrayType::getN(nElem, nMat) )
		{
			fatalErrorInFunction<<
			"sizes of properties do not match a symetric array with size ("<<
			numMaterial_<<"x"<<numMaterial_<<").\n";
			return false;
		}
		else if( numMaterial_ != nMat)
		{
			fatalErrorInFunction<<
			"size mismatch for porperties. \n"<<
			"you supplied "<< numMaterial_<<" items in materials list and "<<
			nMat << " for other properties.\n";
			return false;
		}

		realVector etha_n("etha_n", nElem);
		realVector etha_t("etha_t", nElem);

		ForAll(i , kn)
		{
			etha_n[i] = -2.0*log(en[i])*sqrt(kn[i])/
					sqrt(pow(log(en[i]),static_cast<real>(2.0))+ pow(Pi,static_cast<real>(2.0)));

			etha_t[i] = -2.0*log( et[i]*sqrt(kt[i]) )/
					sqrt(pow(log(et[i]),static_cast<real>(2.0))+ pow(Pi,static_cast<real>(2.0)));
		}

		Vector<TavaresProperties> prop("prop", nElem);
		ForAll(i,kn)
		{
			prop[i] = {kn[i], kt[i], etha_n[i], etha_t[i], mu[i], 
			           E50[i], gamma[i], minBreakageSize[i]};
		}

		TavaresProperties_.assign(prop);

		return true;

	}

	static const char* modelName()
	{
		if constexpr (limited)
		{
			return "TavaresLimited";
		}
		else
		{
			return "TavaresNonLimited";
		}
		return "";
	}

public:


	TypeInfoNV(modelName());

	INLINE_FUNCTION_HD
	Tavares(){}

	Tavares(int32 nMaterial, const ViewType1D<real>& rho, const dictionary& dict)
	:
		numMaterial_(nMaterial),
		rho_("rho",nMaterial),
		TavaresProperties_("TavaresProperties",nMaterial)
	{

		Kokkos::deep_copy(rho_,rho);
		if(!readTavaresDictionary(dict))
		{
			fatalExit;
		}
	}
	
	INLINE_FUNCTION_HD
	Tavares(const Tavares&) = default;

	INLINE_FUNCTION_HD
	Tavares(Tavares&&) = default;

	INLINE_FUNCTION_HD
	Tavares& operator=(const Tavares&) = default;

	INLINE_FUNCTION_HD
	Tavares& operator=(Tavares&&) = default;


	INLINE_FUNCTION_HD
	~Tavares()=default;

	INLINE_FUNCTION_HD
	int32 numMaterial()const
	{
		return numMaterial_;
	}

	//// - Methods

	INLINE_FUNCTION_HD
	void contactForce
	(
		const real dt,
		const uint32 i,
		const uint32 j,
		const uint32 propId_i,
		const uint32 propId_j,
		const real Ri,
		const real Rj,
		const real ovrlp_n,
		const realx3& Vr,
		const realx3& Nij,
		contactForceStorage& history,
		realx3& FCn,
		realx3& FCt
	)const
	{

		auto prop = TavaresProperties_(propId_i,propId_j);

		// Calculate masses
		real mi = 3*Pi/4*pow(Ri,static_cast<real>(3.0))*rho_[propId_i];
		real mj = 3*Pi/4*pow(Rj,static_cast<real>(3.0))*rho_[propId_j];
		real meff = (mi*mj)/(mi+mj);
		real sqrt_meff = sqrt(meff);

		// Calculate relative velocity components
		real vrn = dot(Vr, Nij);	
		realx3 Vt = Vr - vrn*Nij;

		// Update tangential overlap
		history.overlap_t_ += Vt*dt;

		// Calculate contact forces (standard linear model)
		FCn = (-prop.kn_ * ovrlp_n - sqrt_meff * prop.ethan_ * vrn)*Nij;
		FCt = -prop.kt_ * history.overlap_t_ - sqrt_meff * prop.ethat_*Vt;

		// Apply friction limit
		real ft = length(FCt);
		real ft_fric = prop.mu_ * length(FCn);
		
		if(ft > ft_fric)
		{
			if( length(history.overlap_t_) >static_cast<real>(0.0))
			{
				if constexpr (limited)
				{
					FCt *= (ft_fric/ft);
                	history.overlap_t_ = - (FCt/prop.kt_);
				}
				else
				{
					FCt = (ft/ft)*ft_fric;	
				}
			}
			else
			{
				FCt = 0.0;
			}
		}

		// Calculate impact energy for Tavares breakage model
		// E = 0.5 * m_eff * v_rel^2
		real v_rel_squared = dot(Vr, Vr);
		real impact_energy = 0.5 * meff * v_rel_squared;
		
		// Calculate specific energy (energy per unit mass)
		real smaller_mass = (mi < mj) ? mi : mj;
		real specific_energy = impact_energy / smaller_mass;
		
		// Accumulate energy
		history.accumulated_energy_ += specific_energy;
		
		// Calculate breakage probability using Tavares model
		// P(E) = 1 - exp(-(E/E50)^gamma)
		if(specific_energy > 0.0)
		{
			real energy_ratio = specific_energy / prop.E50_;
			real breakage_prob = 1.0 - exp(-pow(energy_ratio, prop.gamma_));
			
			// Check if particle should break (simplified check - in practice,
			// this would trigger particle splitting/fragmentation)
			real smaller_radius = (Ri < Rj) ? Ri : Rj;
			if(breakage_prob > 0.5 && smaller_radius > prop.min_breakage_size_)
			{
				history.breakage_occurred_ = true;
				// In a full implementation, this would trigger:
				// 1. Particle removal from simulation
				// 2. Addition of fragment particles
				// 3. Fragment size distribution calculation
				// This requires coupling with the particle management system
			}
		}

	}
	
	/**
	 * @brief Get breakage status for a contact
	 * @return true if breakage occurred
	 */
	INLINE_FUNCTION_HD
	bool hasBreakageOccurred(const contactForceStorage& history) const
	{
		return history.breakage_occurred_;
	}
	
	/**
	 * @brief Get accumulated energy for a contact
	 * @return accumulated impact energy
	 */
	INLINE_FUNCTION_HD
	real getAccumulatedEnergy(const contactForceStorage& history) const
	{
		return history.accumulated_energy_;
	}
	
};

} //pFlow::cfModels

#endif //__TavaresCF_hpp__
