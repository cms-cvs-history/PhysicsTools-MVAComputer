// -*- C++ -*-
//
// Package:     MVAComputer
// Class  :     ProcLikelihood
// 

// Implementation:
//     A likelihood estimator variable processor. Reads in 0..n values for
//     m variables and calculates the total signal/background likelihood
//     using calibration PDFs for signal and background for each variable.
//     The output variable is set to s/(s+b).
//
// Author:      Christophe Saout
// Created:     Sat Apr 24 15:18 CEST 2007
// $Id: ProcLikelihood.cc,v 1.6 2007/09/17 23:50:38 saout Exp $
//

#include <vector>
#include <memory>

#include "CondFormats/PhysicsToolsObjects/interface/Histogram.h"  
#include "PhysicsTools/MVAComputer/interface/VarProcessor.h"
#include "PhysicsTools/MVAComputer/interface/Calibration.h"
#include "PhysicsTools/MVAComputer/interface/Spline.h"

using namespace PhysicsTools;

namespace { // anonymous

class ProcLikelihood : public VarProcessor {
    public:
	typedef VarProcessor::Registry::Registry<ProcLikelihood,
					Calibration::ProcLikelihood> Registry;

	ProcLikelihood(const char *name,
	               const Calibration::ProcLikelihood *calib,
	               const MVAComputer *computer);
	virtual ~ProcLikelihood() {}

	virtual void configure(ConfIterator iter, unsigned int n);
	virtual void eval(ValueIterator iter, unsigned int n) const;

    private:
	struct PDF {
		virtual ~PDF() {}
		virtual double eval(double value) const = 0;
	};

	struct SplinePDF : public PDF {
		SplinePDF(const Calibration::HistogramF *calib) :
			min(calib->range().min),
			width(calib->range().width())
		{
			std::vector<double> values(
					calib->values().begin() + 1,
					calib->values().end() - 1);
			spline.set(values.size(), &values.front());
		}

		virtual double eval(double value) const;

		double		min, width;
		Spline		spline;
	};

	struct HistogramPDF : public PDF {
		HistogramPDF(const Calibration::HistogramF *calib) :
			histo(calib) {}

		virtual double eval(double value) const;

		const Calibration::HistogramF	*histo;
	};

	struct SigBkg {
		SigBkg(const Calibration::ProcLikelihood::SigBkg &calib)
		{
			if (calib.useSplines) {
				signal = std::auto_ptr<PDF>(
					new SplinePDF(&calib.signal));
				background = std::auto_ptr<PDF>(
					new SplinePDF(&calib.background));
			} else {
				signal = std::auto_ptr<PDF>(
					new HistogramPDF(&calib.signal));
				background = std::auto_ptr<PDF>(
					new HistogramPDF(&calib.background));
			}
		}

		std::auto_ptr<PDF>	signal;
		std::auto_ptr<PDF>	background;
	};

	std::vector<SigBkg>	pdfs;
	int			categoryIdx;
	unsigned int		nCategories;
	double			bias;
};

static ProcLikelihood::Registry registry("ProcLikelihood");

double ProcLikelihood::SplinePDF::eval(double value) const
{
	value = (value - min) / width;
	return spline.eval(value) * spline.numberOfEntries()
	                          / spline.getArea();
}

double ProcLikelihood::HistogramPDF::eval(double value) const
{
	return histo->normalizedValue(value) * histo->numberOfBins();
}

ProcLikelihood::ProcLikelihood(const char *name,
                               const Calibration::ProcLikelihood *calib,
                               const MVAComputer *computer) :
	VarProcessor(name, calib, computer),
	pdfs(calib->pdfs.begin(), calib->pdfs.end()),
	categoryIdx(calib->categoryIdx),
	nCategories(1),
	bias(calib->bias)
{
}

void ProcLikelihood::configure(ConfIterator iter, unsigned int n)
{
	if (categoryIdx >= 0) {
		if ((int)n < categoryIdx + 1)
			return;
		nCategories = pdfs.size() / (n - 1);
		if (nCategories * (n - 1) != pdfs.size())
			return;
	} else if (n != pdfs.size())
		return;

	int i = 0;
	while(iter) {
		if (categoryIdx == i++)
			iter++(Variable::FLAG_NONE);
		else
			iter++(Variable::FLAG_ALL);
	}

	iter << Variable::FLAG_OPTIONAL;
}

void ProcLikelihood::eval(ValueIterator iter, unsigned int n) const
{
	std::vector<SigBkg>::const_iterator pdf;
	std::vector<SigBkg>::const_iterator last;

	if (categoryIdx >= 0) {
		ValueIterator iter2 = iter;
		for(int i = 0; i < categoryIdx; i++)
			++iter2;

		int cat = (int)*iter;
		if (cat < 0 || (unsigned int)cat >= nCategories) {
			iter();
			return;
		}

		pdf = pdfs.begin() + cat * (n - 1);
		last = pdf + (n - 1);
	} else {
		pdf = pdfs.begin();
		last = pdfs.end();
	}		

	int vars = 0;
	double signal = bias;
	double background = 1.0;

	for(int i = 0; pdf != last; ++iter, i++) {
		if (i == categoryIdx)
			continue;
		for(double *value = iter.begin();
		    value < iter.end(); value++) {
			double signalProb =
				std::max(0.0, pdf->signal->eval(*value));
			double backgroundProb =
				std::max(0.0, pdf->background->eval(*value));
			if (signalProb + backgroundProb < 1.0e-30)
				continue;
			vars++;
			signal *= signalProb;
			background *= backgroundProb;
		}
		++pdf;
	}

	if (!vars || signal + background < std::exp(-6 * vars - 2))
		iter();
	else
		iter(signal / (signal + background));
}

} // anonymous namespace
