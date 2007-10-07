// -*- C++ -*-
//
// Package:     MVAComputer
// Class  :     ProcNormalize
// 

// Implementation:
//     Normalizes the input variables. n values in each input variable
//     is normalized to n values for each input variables. The normalization
//     consists of a range normalization step (min...max) and mapping step
//     that equalizes using the probability distribution (via PDF).
//
// Author:      Christophe Saout
// Created:     Sat Apr 24 15:18 CEST 2007
// $Id: ProcNormalize.cc,v 1.6 2007/09/17 23:50:38 saout Exp $
//

#include <vector>

#include "CondFormats/PhysicsToolsObjects/interface/Histogram.h"
#include "PhysicsTools/MVAComputer/interface/VarProcessor.h"
#include "PhysicsTools/MVAComputer/interface/Calibration.h"
#include "PhysicsTools/MVAComputer/interface/Spline.h"

using namespace PhysicsTools;

namespace { // anonymous

class ProcNormalize : public VarProcessor {
    public:
	typedef VarProcessor::Registry::Registry<ProcNormalize,
					Calibration::ProcNormalize> Registry;

	ProcNormalize(const char *name,
	              const Calibration::ProcNormalize *calib,
	              const MVAComputer *computer);
	virtual ~ProcNormalize() {}

	virtual void configure(ConfIterator iter, unsigned int n);
	virtual void eval(ValueIterator iter, unsigned int n) const;

    private:
	struct Map {
		Map(const Calibration::HistogramF &pdf) :
			min(pdf.range().min), width(pdf.range().width())
		{
			std::vector<double> values(
					pdf.values().begin() + 1,
					pdf.values().end() - 1);
			spline.set(values.size(), &values.front());
		}

		double		min, width;
		Spline		spline;
	};

	std::vector<Map>	maps;
	int			categoryIdx;
	unsigned int		nCategories;
};

static ProcNormalize::Registry registry("ProcNormalize");

ProcNormalize::ProcNormalize(const char *name,
                             const Calibration::ProcNormalize *calib,
                             const MVAComputer *computer) :
	VarProcessor(name, calib, computer),
	maps(calib->distr.begin(), calib->distr.end()),
	categoryIdx(calib->categoryIdx),
	nCategories(1)
{
}

void ProcNormalize::configure(ConfIterator iter, unsigned int n)
{
	if (categoryIdx >= 0) {
		if ((int)n < categoryIdx + 1)
			return;
		nCategories = maps.size() / (n - 1);
		if (nCategories * (n - 1) != maps.size())
			return;
	} else if (n != maps.size())
		return;

	int i = 0;
	while(iter) {
		if (categoryIdx == i++)
			iter++(Variable::FLAG_NONE);
		else
			iter << iter++(Variable::FLAG_ALL);
	}
}

void ProcNormalize::eval(ValueIterator iter, unsigned int n) const
{
	std::vector<Map>::const_iterator map;
	std::vector<Map>::const_iterator last;

	if (categoryIdx >= 0) {
		ValueIterator iter2 = iter;
		for(int i = 0; i < categoryIdx; i++)
			++iter2;

		int cat = (int)*iter;
		if (cat < 0 || (unsigned int)cat >= nCategories) {
			for(; iter; ++iter)
				iter();
			return;
		}

		map = maps.begin() + cat * (n - 1);
		last = map + (n - 1);
	} else {
		map = maps.begin();
		last = maps.end();
	}

	for(int i = 0; map != last; ++iter, i++) {
		if (i == categoryIdx)
			continue;
		for(double *value = iter.begin();
		    value < iter.end(); value++) {
			double val = *value;
			val = (val - map->min) / map->width;
			val = map->spline.integral(val);
			iter << val;
		}
		iter();
		++map;
	}
}

} // anonymous namespace
