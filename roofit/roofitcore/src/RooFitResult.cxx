/*****************************************************************************
 * Project: RooFit                                                           *
 * Package: RooFitCore                                                       *
 * @(#)root/roofitcore:$Id$
 * Authors:                                                                  *
 *   WV, Wouter Verkerke, UC Santa Barbara, verkerke@slac.stanford.edu       *
 *   DK, David Kirkby,    UC Irvine,         dkirkby@uci.edu                 *
 *                                                                           *
 * Copyright (c) 2000-2005, Regents of the University of California          *
 *                          and Stanford University. All rights reserved.    *
 *                                                                           *
 * Redistribution and use in source and binary forms,                        *
 * with or without modification, are permitted according to the terms        *
 * listed in LICENSE (http://roofit.sourceforge.net/license.txt)             *
 *****************************************************************************/

//////////////////////////////////////////////////////////////////////////////
/// \class RooFitResult
/// RooFitResult is a container class to hold the input and output
/// of a PDF fit to a dataset. It contains:
///
///   * Values of all constant parameters
///   * Initial and final values of floating parameters with error
///   * Correlation matrix and global correlation coefficients
///   * NLL and EDM at minimum
///
/// No references to the fitted PDF and dataset are stored
///

#include <iostream>
#include <iomanip>

#include "TBuffer.h"
#include "TMath.h"
#include "TMarker.h"
#include "TLine.h"
#include "TBox.h"
#include "TGaxis.h"
#include "TMatrix.h"
#include "TVector.h"
#include "TDirectory.h"
#include "TClass.h"
#include "RooFitResult.h"
#include "RooArgSet.h"
#include "RooArgList.h"
#include "RooRealVar.h"
#include "RooPlot.h"
#include "RooEllipse.h"
#include "RooRandom.h"
#include "RooMsgService.h"
#include "TH2D.h"
#include "TMatrixDSym.h"
#include "RooMultiVarGaussian.h"


using std::ostream, std::string, std::pair, std::vector, std::setw;




////////////////////////////////////////////////////////////////////////////////
/// Constructor with name and title

RooFitResult::RooFitResult(const char *name, const char *title) : TNamed(name, title)
{
   if (name)
      appendToDir(this, true);
}


////////////////////////////////////////////////////////////////////////////////
/// Copy constructor

RooFitResult::RooFitResult(const RooFitResult &other)
   : TNamed(other),
     RooPrintable(other),
     RooDirItem(other),
     _status(other._status),
     _covQual(other._covQual),
     _numBadNLL(other._numBadNLL),
     _minNLL(other._minNLL),
     _edm(other._edm),
     _constPars(new RooArgList),
     _initPars(new RooArgList),
     _finalPars(new RooArgList),
     _statusHistory(other._statusHistory)
{

  other._constPars->snapshot(*_constPars);

  other._initPars->snapshot(*_initPars);

  other._finalPars->snapshot(*_finalPars);
  if (other._randomPars) {
    _randomPars = new RooArgList;
    other._randomPars->snapshot(*_randomPars);
  }
  if (other._Lt) _Lt = new TMatrix(*other._Lt);
  if (other._VM) _VM = new TMatrixDSym(*other._VM) ;
  if (other._CM) _CM = new TMatrixDSym(*other._CM) ;
  if (other._GC) _GC = new TVectorD(*other._GC) ;

  if (GetName())
    appendToDir(this, true);
}



////////////////////////////////////////////////////////////////////////////////
/// Destructor

RooFitResult::~RooFitResult()
{
  if (_constPars) delete _constPars ;
  if (_initPars)  delete _initPars ;
  if (_finalPars) delete _finalPars ;
  if (_globalCorr) delete _globalCorr;
  if (_randomPars) delete _randomPars;
  if (_Lt) delete _Lt;
  if (_CM) delete _CM ;
  if (_VM) delete _VM ;
  if (_GC) delete _GC ;

  _corrMatrix.RemoveAll();
  _corrMatrix.Delete();

  removeFromDir(this) ;
}


////////////////////////////////////////////////////////////////////////////////
/// Fill the list of constant parameters

void RooFitResult::setConstParList(const RooArgList& list)
{
  if (_constPars) delete _constPars ;
  _constPars = new RooArgList;
  list.snapshot(*_constPars);
  for(auto* rrv : dynamic_range_cast<RooRealVar*>(*_constPars)) {
    if (rrv) {
      rrv->deleteSharedProperties() ;
    }
  }
}



////////////////////////////////////////////////////////////////////////////////
/// Fill the list of initial values of the floating parameters

void RooFitResult::setInitParList(const RooArgList& list)
{
  if (_initPars) delete _initPars ;
  _initPars = new RooArgList;
  list.snapshot(*_initPars);
  for(auto* rrv : dynamic_range_cast<RooRealVar*>(*_initPars)) {
    if (rrv) {
      rrv->deleteSharedProperties() ;
    }
  }
}



////////////////////////////////////////////////////////////////////////////////
/// Fill the list of final values of the floating parameters

void RooFitResult::setFinalParList(const RooArgList& list)
{
  if (_finalPars) delete _finalPars ;
  _finalPars = new RooArgList;
  list.snapshot(*_finalPars);

  for(auto* rrv : dynamic_range_cast<RooRealVar*>(*_finalPars)) {
    if (rrv) {
      rrv->deleteSharedProperties() ;
    }
  }
}



////////////////////////////////////////////////////////////////////////////////

Int_t RooFitResult::statusCodeHistory(UInt_t icycle) const
{
  if (icycle>=_statusHistory.size()) {
    coutE(InputArguments) << "RooFitResult::statusCodeHistory(" << GetName()
           << " ERROR request for status history slot "
           << icycle << " exceeds history count of " << _statusHistory.size() << std::endl ;
  }
  return _statusHistory[icycle].second ;
}



////////////////////////////////////////////////////////////////////////////////

const char* RooFitResult::statusLabelHistory(UInt_t icycle) const
{
  if (icycle>=_statusHistory.size()) {
    coutE(InputArguments) << "RooFitResult::statusLabelHistory(" << GetName()
           << " ERROR request for status history slot "
           << icycle << " exceeds history count of " << _statusHistory.size() << std::endl ;
  }
  return _statusHistory[icycle].first.c_str() ;
}



////////////////////////////////////////////////////////////////////////////////
/// Add objects to a 2D plot that represent the fit results for the
/// two named parameters.  The input frame with the objects added is
/// returned, or zero in case of an error.  Which objects are added
/// are determined by the options string which should be a concatenation
/// of the following (not case sensitive):
///
/// *  M - a marker at the best fit result
/// *  E - an error ellipse calculated at 39%CL using the error matrix at the minimum
/// *  1 - the 1-sigma error bar for parameter 1
/// *  2 - the 1-sigma error bar for parameter 2
/// *  B - the bounding box for the error ellipse
/// *  H - a line and horizontal axis for reading off the correlation coefficient
/// *  V - a line and vertical axis for reading off the correlation coefficient
/// *  A - draw axes for reading off the correlation coefficients with the H or V options
///
/// You can change the attributes of objects in the returned RooPlot using the
/// various `RooPlot::getAttXxx(name)` member functions, e.g.
/// ```
///   plot->getAttLine("contour")->SetLineStyle(kDashed);
/// ```
/// Use `plot->Print()` for a list of all objects and their names (unfortunately most
/// of the ROOT builtin graphics objects like TLine are unnamed). Drag the left mouse
/// button along the labels of either axis button to interactively zoom in a plot.

RooPlot *RooFitResult::plotOn(RooPlot *frame, const char *parName1, const char *parName2,
               const char *options) const
{
  // lookup the input parameters by name: we require that they were floated in our fit
  const RooRealVar *par1= dynamic_cast<const RooRealVar*>(floatParsFinal().find(parName1));
  if(nullptr == par1) {
    coutE(InputArguments) << "RooFitResult::correlationPlot: parameter not floated in fit: " << parName1 << std::endl;
    return nullptr;
  }
  const RooRealVar *par2= dynamic_cast<const RooRealVar*>(floatParsFinal().find(parName2));
  if(nullptr == par2) {
    coutE(InputArguments) << "RooFitResult::correlationPlot: parameter not floated in fit: " << parName2 << std::endl;
    return nullptr;
  }

  // options are not case sensitive
  TString opt(options);
  opt.ToUpper();

  // lookup the 2x2 covariance matrix elements for these variables
  double x1= par1->getVal();
  double x2= par2->getVal();
  double s1= par1->getError();
  double s2= par2->getError();
  double rho= correlation(parName1, parName2);

  // add a 39%CL error ellipse, if requested
  if(opt.Contains("E")) {
    RooEllipse *contour= new RooEllipse("contour",x1,x2,s1,s2,rho,100,1);
    contour->SetLineWidth(2) ;
    frame->addPlotable(contour);
  }

  // add the error bar for parameter 1, if requested
  if(opt.Contains("1")) {
    TLine *hline= new TLine(x1-s1,x2,x1+s1,x2);
    hline->SetLineColor(kRed);
    frame->addObject(hline);
  }

  if(opt.Contains("2")) {
    TLine *vline= new TLine(x1,x2-s2,x1,x2+s2);
    vline->SetLineColor(kRed);
    frame->addObject(vline);
  }

  if(opt.Contains("B")) {
    TBox *box= new TBox(x1-s1,x2-s2,x1+s1,x2+s2);
    box->SetLineStyle(kDashed);
    box->SetLineColor(kRed);
    box->SetFillStyle(0);
    frame->addObject(box);
  }

  if(opt.Contains("H")) {
    TLine *line= new TLine(x1-rho*s1,x2-s2,x1+rho*s1,x2+s2);
    line->SetLineStyle(kDashed);
    line->SetLineColor(kBlue);
    line->SetLineWidth(2) ;
    frame->addObject(line);
    if(opt.Contains("A")) {
      TGaxis *axis= new TGaxis(x1-s1,x2-s2,x1+s1,x2-s2,-1.,+1.,502,"-=");
      axis->SetLineColor(kBlue);
      frame->addObject(axis);
    }
  }

  if(opt.Contains("V")) {
    TLine *line= new TLine(x1-s1,x2-rho*s2,x1+s1,x2+rho*s2);
    line->SetLineStyle(kDashed);
    line->SetLineColor(kBlue);
    line->SetLineWidth(2) ;
    frame->addObject(line);
    if(opt.Contains("A")) {
      TGaxis *axis= new TGaxis(x1-s1,x2-s2,x1-s1,x2+s2,-1.,+1.,502,"-=");
      axis->SetLineColor(kBlue);
      frame->addObject(axis);
    }
  }

  // add a marker at the fitted value, if requested
  if(opt.Contains("M")) {
    TMarker *marker= new TMarker(x1,x2,20);
    marker->SetMarkerColor(kBlack);
    frame->addObject(marker);
  }

  return frame;
}


////////////////////////////////////////////////////////////////////////////////
/// Return a list of floating parameter values that are perturbed from the final
/// fit values by random amounts sampled from the covariance matrix. The returned
/// object is overwritten with each call and belongs to the RooFitResult. Uses
/// the "square root method" to decompose the covariance matrix, which makes inverting
/// it unnecessary.

const RooArgList& RooFitResult::randomizePars() const
{
  Int_t nPar= _finalPars->size();
  if(nullptr == _randomPars) { // first-time initialization
    assert(nullptr != _finalPars);
    // create the list of random values to fill
    _randomPars = new RooArgList;
    _finalPars->snapshot(*_randomPars);
    // calculate the elements of the upper-triangular matrix L that gives Lt*L = C
    // where Lt is the transpose of L (the "square-root method")
    TMatrix L(nPar,nPar);
    for(Int_t iPar= 0; iPar < nPar; iPar++) {
      // calculate the diagonal term first
      L(iPar,iPar)= covariance(iPar,iPar);
      for(Int_t k= 0; k < iPar; k++) {
   double tmp= L(k,iPar);
   L(iPar,iPar)-= tmp*tmp;
      }
      L(iPar,iPar)= sqrt(L(iPar,iPar));
      // then the off-diagonal terms
      for(Int_t jPar= iPar+1; jPar < nPar; jPar++) {
   L(iPar,jPar)= covariance(iPar,jPar);
   for(Int_t k= 0; k < iPar; k++) {
     L(iPar,jPar)-= L(k,iPar)*L(k,jPar);
   }
   L(iPar,jPar)/= L(iPar,iPar);
      }
    }
    // remember Lt
    _Lt= new TMatrix(TMatrix::kTransposed,L);
  }
  else {
    // reset to the final fit values
    _randomPars->assign(*_finalPars);
  }

  // create a vector of unit Gaussian variables
  TVector g(nPar);
  for(Int_t k= 0; k < nPar; k++) g(k)= RooRandom::gaussian();
  // multiply this vector by Lt to introduce the appropriate correlations
  g*= (*_Lt);
  // add the mean value offsets and store the results
  Int_t index(0);
  for(auto * par : static_range_cast<RooRealVar*>(*_randomPars)) {
    par->setVal(par->getVal() + g(index++));
  }

  return *_randomPars;
}


////////////////////////////////////////////////////////////////////////////////
/// Return the correlation between parameters 'par1' and 'par2'

double RooFitResult::correlation(const char* parname1, const char* parname2) const
{
  Int_t idx1 = _finalPars->index(parname1) ;
  Int_t idx2 = _finalPars->index(parname2) ;
  if (idx1<0) {
    coutE(InputArguments) << "RooFitResult::correlation(" << GetName() << ") parameter " << parname1 << " is not a floating fit parameter" << std::endl ;
    return 0 ;
  }
  if (idx2<0) {
    coutE(InputArguments) << "RooFitResult::correlation(" << GetName() << ") parameter " << parname2 << " is not a floating fit parameter" << std::endl ;
    return 0 ;
  }
  return correlation(idx1,idx2) ;
}



////////////////////////////////////////////////////////////////////////////////
/// Return the set of correlation coefficients of parameter 'par' with
/// all other floating parameters

const RooArgList* RooFitResult::correlation(const char* parname) const
{
  if (_globalCorr==nullptr) {
    fillLegacyCorrMatrix() ;
  }

  RooAbsArg* arg = _initPars->find(parname) ;
  if (!arg) {
    coutE(InputArguments) << "RooFitResult::correlation: variable " << parname << " not a floating parameter in fit" << std::endl ;
    return nullptr ;
  }
  return static_cast<RooArgList*>(_corrMatrix.At(_initPars->index(arg))) ;
}



////////////////////////////////////////////////////////////////////////////////
/// Return the global correlation of the named parameter

double RooFitResult::globalCorr(const char* parname)
{
  if (_globalCorr==nullptr) {
    fillLegacyCorrMatrix() ;
  }

  RooAbsArg* arg = _initPars->find(parname) ;
  if (!arg) {
    coutE(InputArguments) << "RooFitResult::globalCorr: variable " << parname << " not a floating parameter in fit" << std::endl ;
    return 0 ;
  }

  if (_globalCorr) {
    return (static_cast<RooAbsReal*>(_globalCorr->at(_initPars->index(arg))))->getVal() ;
  } else {
    return 1.0 ;
  }
}



////////////////////////////////////////////////////////////////////////////////
/// Return the list of all global correlations

const RooArgList* RooFitResult::globalCorr()
{
  if (_globalCorr==nullptr) {
    fillLegacyCorrMatrix() ;
  }

  return _globalCorr ;
}



////////////////////////////////////////////////////////////////////////////////
/// Return a correlation matrix element addressed with numeric indices.

double RooFitResult::correlation(Int_t row, Int_t col) const
{
  return (*_CM)(row,col) ;
}


////////////////////////////////////////////////////////////////////////////////
/// Return the covariance matrix element addressed with numeric indices.

double RooFitResult::covariance(Int_t row, Int_t col) const
{
  return (*_VM)(row,col) ;
}



////////////////////////////////////////////////////////////////////////////////
/// Print fit result to stream 'os'. In Verbose mode, the constant parameters and
/// the initial and final values of the floating parameters are printed.
/// Standard mode only the final values of the floating parameters are printed

void RooFitResult::printMultiline(ostream& os, Int_t /*contents*/, bool verbose, TString indent) const
{

  os << std::endl
     << indent << "  RooFitResult: minimized FCN value: " << _minNLL << ", estimated distance to minimum: " << _edm << std::endl
     << indent << "                covariance matrix quality: " ;
  switch(_covQual) {
  case -1 : os << "Unknown, matrix was externally provided" ; break ;
  case 0  : os << "Not calculated at all" ; break ;
  case 1  : os << "Approximation only, not accurate" ; break ;
  case 2  : os << "Full matrix, but forced positive-definite" ; break ;
  case 3  : os << "Full, accurate covariance matrix" ; break ;
  }
  os << std::endl ;
  os << indent << "                Status : " ;
  for (vector<pair<string,int> >::const_iterator iter = _statusHistory.begin() ; iter != _statusHistory.end() ; ++iter) {
    os << iter->first << "=" << iter->second << " " ;
  }
  os << std::endl << std::endl;

  if (verbose) {
    if (!_constPars->empty()) {
      os << indent << "    Constant Parameter    Value     " << std::endl
    << indent << "  --------------------  ------------" << std::endl ;

      for (std::size_t i=0 ; i<_constPars->size() ; i++) {
        os << indent << "  " << setw(20) << _constPars->at(i)->GetName() << "  " << setw(12);
        if(RooRealVar* v = dynamic_cast<RooRealVar*>(_constPars->at(i))) {
         os << TString::Format("%12.4e",v->getVal());
        } else {
          _constPars->at(i)->printValue(os); // for anything other than RooRealVar use printValue method to print
        }
        os << std::endl ;
      }

      os << std::endl ;
    }

    // Has any parameter asymmetric errors?
    bool doAsymErr(false) ;
    for (std::size_t i=0 ; i<_finalPars->size() ; i++) {
      if (static_cast<RooRealVar*>(_finalPars->at(i))->hasAsymError()) {
   doAsymErr=true ;
   break ;
      }
    }

    if (doAsymErr) {
      os << indent << "    Floating Parameter  InitialValue    FinalValue (+HiError,-LoError)    GblCorr." << std::endl
    << indent << "  --------------------  ------------  ----------------------------------  --------" << std::endl ;
    } else {
      os << indent << "    Floating Parameter  InitialValue    FinalValue +/-  Error     GblCorr." << std::endl
    << indent << "  --------------------  ------------  --------------------------  --------" << std::endl ;
    }

    for (std::size_t i=0 ; i<_finalPars->size() ; i++) {
      os << indent << "  "    << setw(20) << ((RooAbsArg*)_finalPars->at(i))->GetName() ;
      os << indent << "  "    << setw(12) << Form("%12.4e",(static_cast<RooRealVar*>(_initPars->at(i)))->getVal())
    << indent << "  "    << setw(12) << Form("%12.4e",(static_cast<RooRealVar*>(_finalPars->at(i)))->getVal()) ;

      if ((static_cast<RooRealVar*>(_finalPars->at(i)))->hasAsymError()) {
   os << setw(21) << Form(" (+%8.2e,-%8.2e)",(static_cast<RooRealVar*>(_finalPars->at(i)))->getAsymErrorHi(),
                          -1*(static_cast<RooRealVar*>(_finalPars->at(i)))->getAsymErrorLo()) ;
      } else {
   double err = (static_cast<RooRealVar*>(_finalPars->at(i)))->getError() ;
   os << (doAsymErr?"        ":"") << " +/- " << setw(9)  << Form("%9.2e",err) ;
      }

      if (_globalCorr) {
   os << "  "    << setw(8)  << Form("%8.6f" ,(static_cast<RooRealVar*>(_globalCorr->at(i)))->getVal()) ;
      } else {
   os << "  <none>" ;
      }

      os << std::endl ;
    }

  } else {
    os << indent << "    Floating Parameter    FinalValue +/-  Error   " << std::endl
       << indent << "  --------------------  --------------------------" << std::endl ;

    for (std::size_t i=0 ; i<_finalPars->size() ; i++) {
      double err = (static_cast<RooRealVar*>(_finalPars->at(i)))->getError() ;
      os << indent << "  "    << setw(20) << ((RooAbsArg*)_finalPars->at(i))->GetName()
    << "  "    << setw(12) << Form("%12.4e",(static_cast<RooRealVar*>(_finalPars->at(i)))->getVal())
    << " +/- " << setw(9)  << Form("%9.2e",err)
    << std::endl ;
    }
  }


  os << std::endl ;
}


////////////////////////////////////////////////////////////////////////////////
/// Function called by RooMinimizer

void RooFitResult::fillCorrMatrix(const std::vector<double>& globalCC, const TMatrixDSym& corrs, const TMatrixDSym& covs)
{
  // Sanity check
  if (globalCC.empty() || corrs.GetNoElements() < 1 || covs.GetNoElements() < 1) {
    coutI(Minimization) << "RooFitResult::fillCorrMatrix: number of floating parameters is zero, correlation matrix not filled" << std::endl ;
    return ;
  }

  if (!_initPars) {
    coutE(Minimization) << "RooFitResult::fillCorrMatrix: ERROR: list of initial parameters must be filled first" << std::endl ;
    return ;
  }

  // Delete eventual previous correlation data holders
  if (_CM) delete _CM ;
  if (_VM) delete _VM ;
  if (_GC) delete _GC ;

  // Build holding arrays for correlation coefficients
  _CM = new TMatrixDSym(corrs) ;
  _VM = new TMatrixDSym(covs) ;
  _GC = new TVectorD(_CM->GetNcols()) ;
  for(int i=0 ; i<_CM->GetNcols() ; i++) {
    (*_GC)[i] = globalCC[i] ;
  }
  //fillLegacyCorrMatrix() ;
}





////////////////////////////////////////////////////////////////////////////////
/// Sanity check

void RooFitResult::fillLegacyCorrMatrix() const
{
  if (!_CM) return ;

  // Delete eventual previous correlation data holders
  if (_globalCorr) delete _globalCorr ;
  _corrMatrix.Delete();

  // Build holding arrays for correlation coefficients
  _globalCorr = new RooArgList("globalCorrelations") ;

  for(RooAbsArg* arg : *_initPars) {
    // Create global correlation value holder
    TString gcName("GC[") ;
    gcName.Append(arg->GetName()) ;
    gcName.Append("]") ;
    TString gcTitle(arg->GetTitle()) ;
    gcTitle.Append(" Global Correlation") ;
    _globalCorr->addOwned(std::make_unique<RooRealVar>(gcName.Data(),gcTitle.Data(),0.));

    // Create array with correlation holders for this parameter
    TString name("C[") ;
    name.Append(arg->GetName()) ;
    name.Append(",*]") ;
    RooArgList* corrMatrixRow = new RooArgList(name.Data()) ;
    _corrMatrix.Add(corrMatrixRow) ;
    for(RooAbsArg* arg2 : *_initPars) {

      TString cName("C[") ;
      cName.Append(arg->GetName()) ;
      cName.Append(",") ;
      cName.Append(arg2->GetName()) ;
      cName.Append("]") ;
      TString cTitle("Correlation between ") ;
      cTitle.Append(arg->GetName()) ;
      cTitle.Append(" and ") ;
      cTitle.Append(arg2->GetName()) ;
      corrMatrixRow->addOwned(std::make_unique<RooRealVar>(cName.Data(),cTitle.Data(),0.));
    }
  }

  if (!_GC) return ;

  for (unsigned int i = 0; i < static_cast<unsigned int>(_corrMatrix.GetSize()) ; ++i) {

    // Find the next global correlation slot to fill, skipping fixed parameters
    auto& gcVal = static_cast<RooRealVar&>((*_globalCorr)[i]);
    gcVal.setVal((*_GC)(i)) ; // WVE FIX THIS

    // Fill a row of the correlation matrix
    auto corrMatrixCol = static_cast<RooArgList const&>(*_corrMatrix.At(i));
    for (unsigned int it = 0; it < corrMatrixCol.size() ; ++it) {
      auto& cVal = static_cast<RooRealVar&>(corrMatrixCol[it]);
      double value = (*_CM)(i,it) ;
      cVal.setVal(value);
      (*_CM)(i,it) = value;
    }
  }
}


////////////////////////////////////////////////////////////////////////////////

void RooFitResult::fillPrefitCorrMatrix()
{

   // Delete eventual previous correlation data holders
   if (_CM)
      delete _CM;
   if (_VM)
      delete _VM;
   if (_GC)
      delete _GC;

   // Build holding arrays for correlation coefficients
   _CM = new TMatrixDSym(_initPars->size());
   _VM = new TMatrixDSym(_initPars->size());
   _GC = new TVectorD(_initPars->size());

   for (std::size_t ii = 0; ii < _finalPars->size(); ii++) {
      (*_CM)(ii, ii) = 1;
      (*_VM)(ii, ii) = static_cast<RooRealVar *>(_finalPars->at(ii))->getError() * static_cast<RooRealVar *>(_finalPars->at(ii))->getError();
      (*_GC)(ii) = 0;
   }
}


namespace {

void isIdenticalErrMsg(std::string const& msgHead, const RooAbsReal* tv, const RooAbsReal* ov, bool verbose) {
  if(!verbose) return;
  std::cout << "RooFitResult::isIdentical: " << msgHead << " " << tv->GetName() << " differs in value:\t"
      << tv->getVal() << " vs.\t" << ov->getVal()
      << "\t(" << (tv->getVal()-ov->getVal())/ov->getVal() << ")" << std::endl;
}

void isErrorIdenticalErrMsg(std::string const& msgHead, const RooRealVar* tv, const RooRealVar* ov, bool verbose) {
  if(!verbose) return;
  std::cout << "RooFitResult::isIdentical: " << msgHead << " " << tv->GetName() << " differs in error:\t"
      << tv->getError() << " vs.\t" << ov->getError()
      << "\t(" << (tv->getError()-ov->getError())/ov->getError() << ")" << std::endl;
}

} // namespace


////////////////////////////////////////////////////////////////////////////////
/// Return true if this fit result is identical to other within tolerances, ignoring the correlation matrix.
/// \param[in] other Fit result to test against.
/// \param[in] tol **Relative** tolerance for parameters and NLL.
/// \param[in] tolErr **Relative** tolerance for parameter errors.
/// \param[in] verbose If this function will log to the standard output when comparisons fail.

bool RooFitResult::isIdenticalNoCov(const RooFitResult& other, double tol, double tolErr, bool verbose) const
{
  bool ret = true;
  auto deviation = [](const double left, const double right, double tolerance){
    return right != 0. ? std::abs((left - right)/right) >= tolerance : std::abs(left) >= tolerance;
  };

  auto compare = [&](RooArgList const& pars, RooArgList const& otherpars, std::string const& prefix, bool isVerbose) {
    bool out = true;

    for (auto * tv : static_range_cast<const RooAbsReal*>(pars)) {
      auto ov = static_cast<const RooAbsReal*>(otherpars.find(tv->GetName())) ;

      // Check in the parameter is in the other fit result
      if (!ov) {
        if(verbose) std::cout << "RooFitResult::isIdentical: cannot find " << prefix << " " << tv->GetName() << " in reference" << std::endl ;
        out = false;
      }

      // Compare parameter value
      if (ov && deviation(tv->getVal(), ov->getVal(), tol)) {
        isIdenticalErrMsg(prefix, tv, ov, isVerbose);
        out = false;
      }

      // Compare parameter error if it's a RooRealVar
      auto * rtv = dynamic_cast<RooRealVar const*>(tv);
      auto * rov = dynamic_cast<RooRealVar const*>(ov);
      if(rtv && rov) {
        if (ov && deviation(rtv->getError(), rov->getError(), tolErr)) {
          isErrorIdenticalErrMsg(prefix, rtv, rov, isVerbose);
          out = false;
        }
      }
    }

    return out;
  };

  if (deviation(_minNLL, other._minNLL, tol)) {
    if(verbose) std::cout << "RooFitResult::isIdentical: minimized value of -log(L) is different " << _minNLL << " vs. " << other._minNLL << std::endl;
    ret = false;
  }

  ret &= compare(*_constPars, *other._constPars, "constant parameter", verbose);
  ret &= compare(*_initPars, *other._initPars, "initial parameter", verbose);
  ret &= compare(*_finalPars, *other._finalPars, "final parameter", verbose);

  return ret;
}


////////////////////////////////////////////////////////////////////////////////
/// Return true if this fit result is identical to other within tolerances.
/// \param[in] other Fit result to test against.
/// \param[in] tol **Relative** tolerance for parameters and NLL.
/// \param[in] tolCorr **absolute** tolerance for correlation coefficients.
/// \param[in] verbose If this function will log to the standard output when comparisons fail.
///
/// As the relative tolerance for the parameter errors, the default value of
/// `1e-3` will be used.

bool RooFitResult::isIdentical(const RooFitResult& other, double tol, double tolCorr, bool verbose) const
{
  bool ret = isIdenticalNoCov(other, tol, 1e-3 /* synced with default parameter*/, verbose);

  auto deviationCorr = [tolCorr](const double left, const double right){
    return std::abs(left - right) >= tolCorr;
  };

  // Only examine correlations for cases with >1 floating parameter
  if (_finalPars->size()>1) {

    fillLegacyCorrMatrix() ;
    other.fillLegacyCorrMatrix() ;

    for (std::size_t i=0 ; i<_globalCorr->size() ; i++) {
      auto tv = static_cast<const RooAbsReal*>(_globalCorr->at(i));
      auto ov = static_cast<const RooAbsReal*>(other._globalCorr->find(_globalCorr->at(i)->GetName())) ;
      if (!ov) {
        if(verbose) std::cout << "RooFitResult::isIdentical: cannot find global correlation coefficient " << tv->GetName() << " in reference" << std::endl ;
        ret = false ;
      }
      if (ov && deviationCorr(tv->getVal(), ov->getVal())) {
        isIdenticalErrMsg("global correlation coefficient", tv, ov, verbose);
        ret = false ;
      }
    }

    for (Int_t j=0 ; j<_corrMatrix.GetSize() ; j++) {
      RooArgList* row = static_cast<RooArgList*>(_corrMatrix.At(j)) ;
      RooArgList* orow = static_cast<RooArgList*>(other._corrMatrix.At(j)) ;
      for (std::size_t i=0 ; i<row->size() ; i++) {
        auto tv = static_cast<const RooAbsReal*>(row->at(i));
        auto ov = static_cast<const RooAbsReal*>(orow->find(tv->GetName())) ;
        if (!ov) {
          if(verbose) std::cout << "RooFitResult::isIdentical: cannot find correlation coefficient " << tv->GetName() << " in reference" << std::endl ;
          ret = false ;
        }
        if (ov && deviationCorr(tv->getVal(), ov->getVal())) {
          isIdenticalErrMsg("correlation coefficient", tv, ov, verbose);
          ret = false ;
        }
      }
    }
  }

  return ret ;
}


////////////////////////////////////////////////////////////////////////////////
/// Import the results of the last fit performed by gMinuit, interpreting
/// the fit parameters as the given varList of parameters.

RooFitResult *RooFitResult::prefitResult(const RooArgList &paramList)
{
   // Verify that all members of varList are of type RooRealVar
   for(RooAbsArg * arg : paramList) {
      if (!dynamic_cast<RooRealVar *>(arg)) {
         oocoutE(nullptr, InputArguments) << "RooFitResult::lastMinuitFit: ERROR: variable '" << arg->GetName()
                                               << "' is not of type RooRealVar" << std::endl;
         return nullptr;
      }
   }

   RooFitResult *r = new RooFitResult("lastMinuitFit", "Last MINUIT fit");

   // Extract names of fit parameters from MINUIT
   // and construct corresponding RooRealVars
   RooArgList constPars("constPars");
   RooArgList floatPars("floatPars");

   for(RooAbsArg* arg : paramList) {
      if (arg->isConstant()) {
         constPars.addClone(*arg);
      } else {
         floatPars.addClone(*arg);
      }
   }

   r->setConstParList(constPars);
   r->setInitParList(floatPars);
   r->setFinalParList(floatPars);
   r->setMinNLL(0);
   r->setEDM(0);
   r->setCovQual(0);
   r->setStatus(0);
   r->fillPrefitCorrMatrix();

   return r;
}

////////////////////////////////////////////////////////////////////////////////
/// Store externally provided correlation matrix in this RooFitResult ;

void RooFitResult::setCovarianceMatrix(TMatrixDSym& V)
{
  // Delete any previous matrices
  if (_VM) {
    delete _VM ;
  }
  if (_CM) {
    delete _CM ;
  }

  // Clone input covariance matrix ;
  _VM = static_cast<TMatrixDSym*>(V.Clone()) ;

  // Now construct correlation matrix from it
  _CM = static_cast<TMatrixDSym*>(_VM->Clone()) ;
  for (Int_t i=0 ; i<_CM->GetNrows() ; i++) {
    for (Int_t j=0 ; j<_CM->GetNcols() ; j++) {
      if (i!=j) {
   (*_CM)(i,j) = (*_CM)(i,j) / sqrt((*_CM)(i,i)*(*_CM)(j,j)) ;
      }
    }
  }
  for (Int_t i=0 ; i<_CM->GetNrows() ; i++) {
    (*_CM)(i,i) = 1.0 ;
  }

  _covQual = -1 ;
}



////////////////////////////////////////////////////////////////////////////////
/// Return TH2D of correlation matrix

TH2* RooFitResult::correlationHist(const char* name) const
{
  Int_t n = _CM->GetNcols() ;

  TH2D* hh = new TH2D(name,name,n,0,n,n,0,n) ;

  for (Int_t i = 0 ; i<n ; i++) {
    for (Int_t j = 0 ; j<n; j++) {
      hh->Fill(i+0.5,n-j-0.5,(*_CM)(i,j)) ;
    }
    hh->GetXaxis()->SetBinLabel(i+1,_finalPars->at(i)->GetName()) ;
    hh->GetYaxis()->SetBinLabel(n-i,_finalPars->at(i)->GetName()) ;
  }
  hh->SetMinimum(-1) ;
  hh->SetMaximum(+1) ;


  return hh ;
}




////////////////////////////////////////////////////////////////////////////////
/// Return covariance matrix

const TMatrixDSym& RooFitResult::covarianceMatrix() const
{
  return *_VM ;
}




////////////////////////////////////////////////////////////////////////////////
/// Return a reduced covariance matrix (Note that Vred _is_ a simple sub-matrix of V,
/// row/columns are ordered to matched the convention given in input argument 'params'

TMatrixDSym RooFitResult::reducedCovarianceMatrix(const RooArgList& params) const
{
  const TMatrixDSym& V = covarianceMatrix() ;


  // Make sure that all given params were floating parameters in the represented fit
  RooArgList params2 ;
  for(RooAbsArg* arg : params) {
    if (_finalPars->find(arg->GetName())) {
      params2.add(*arg) ;
    } else {
      coutW(InputArguments) << "RooFitResult::reducedCovarianceMatrix(" << GetName() << ") WARNING input variable "
             << arg->GetName() << " was not a floating parameters in fit result and is ignored" << std::endl ;
    }
  }

   // fix for bug ROOT-8044
   // use same order given bby vector params
   vector<int> indexMap(params2.size());
   for (std::size_t i=0 ; i<params2.size() ; i++) {
      indexMap[i] = _finalPars->index(params2[i].GetName());
      assert(indexMap[i] < V.GetNrows());
   }

   TMatrixDSym Vred(indexMap.size());
   for (int i = 0; i < Vred.GetNrows(); ++i) {
      for (int j = 0; j < Vred.GetNcols(); ++j) {
         Vred(i,j) = V( indexMap[i], indexMap[j]);
      }
   }
   return Vred;
}



////////////////////////////////////////////////////////////////////////////////
/// Return a reduced covariance matrix, which is calculated as
/// \f[
///   V_\mathrm{red} = \bar{V_{22}} = V_{11} - V_{12} \cdot V_{22}^{-1} \cdot V_{21},
/// \f]
/// where \f$ V_{11},V_{12},V_{21},V_{22} \f$ represent a block decomposition of the covariance matrix into observables that
/// are propagated (labeled by index '1') and that are not propagated (labeled by index '2'), and \f$ \bar{V_{22}} \f$
/// is the Shur complement of \f$ V_{22} \f$, calculated as shown above.
///
/// (Note that \f$ V_\mathrm{red} \f$ is *not* a simple sub-matrix of \f$ V \f$)

TMatrixDSym RooFitResult::conditionalCovarianceMatrix(const RooArgList& params) const
{
  const TMatrixDSym& V = covarianceMatrix() ;

  // Handle case where V==Vred here
  if (V.GetNcols()==int(params.size())) {
    return V ;
  }

  double det = V.Determinant() ;

  if (det<=0) {
    coutE(Eval) << "RooFitResult::conditionalCovarianceMatrix(" << GetName() << ") ERROR: covariance matrix is not positive definite (|V|="
      << det << ") cannot reduce it" << std::endl ;
    throw string("RooFitResult::conditionalCovarianceMatrix() ERROR, input covariance matrix is not positive definite") ;
  }

  // Make sure that all given params were floating parameters in the represented fit
  RooArgList params2 ;
  for(RooAbsArg* arg : params) {
    if (_finalPars->find(arg->GetName())) {
      params2.add(*arg) ;
    } else {
      coutW(InputArguments) << "RooFitResult::conditionalCovarianceMatrix(" << GetName() << ") WARNING input variable "
             << arg->GetName() << " was not a floating parameters in fit result and is ignored" << std::endl ;
    }
  }

  // Need to order params in vector in same order as in covariance matrix
  RooArgList params3 ;
  for(RooAbsArg* arg : *_finalPars) {
    if (params2.find(arg->GetName())) {
      params3.add(*arg) ;
    }
  }

  // Find (subset) of parameters that are stored in the covariance matrix
  vector<int> map1;
  vector<int> map2;
  for (std::size_t i=0 ; i<_finalPars->size() ; i++) {
    if (params3.find(_finalPars->at(i)->GetName())) {
      map1.push_back(i) ;
    } else {
      map2.push_back(i) ;
    }
  }

  // Rearrange matrix in block form with 'params' first and 'others' last
  // (preserving relative order)
  TMatrixDSym S11;
  TMatrixDSym S22;
  TMatrixD S12;
  TMatrixD S21;
  RooMultiVarGaussian::blockDecompose(V,map1,map2,S11,S12,S21,S22) ;

  // Constructed conditional matrix form         -1
  // F(X1|X2) --> CovI --> S22bar = S11 - S12 S22  S21

  // Do eigenvalue decomposition
  TMatrixD S22Inv(TMatrixD::kInverted,S22) ;
  TMatrixD S22bar =  S11 - S12 * (S22Inv * S21) ;

  // Convert explicitly to symmetric form
  TMatrixDSym Vred(S22bar.GetNcols()) ;
  for (int i=0 ; i<Vred.GetNcols() ; i++) {
    for (int j=i ; j<Vred.GetNcols() ; j++) {
      Vred(i,j) = (S22bar(i,j) + S22bar(j,i))/2 ;
      Vred(j,i) = Vred(i,j) ;
    }
  }

  return Vred ;
}



////////////////////////////////////////////////////////////////////////////////
/// Return correlation matrix ;

const TMatrixDSym& RooFitResult::correlationMatrix() const
{
  return *_CM ;
}



////////////////////////////////////////////////////////////////////////////////
/// Return a p.d.f that represents the fit result as a multi-variate probability densisty
/// function on the floating fit parameters, including correlations

RooAbsPdf* RooFitResult::createHessePdf(const RooArgSet& params) const
{
  const TMatrixDSym& V = covarianceMatrix() ;
  double det = V.Determinant() ;

  if (det<=0) {
    coutE(Eval) << "RooFitResult::createHessePdf(" << GetName() << ") ERROR: covariance matrix is not positive definite (|V|="
      << det << ") cannot construct p.d.f" << std::endl ;
    return nullptr ;
  }

  // Make sure that all given params were floating parameters in the represented fit
  RooArgList params2 ;
  for(RooAbsArg* arg : params) {
    if (_finalPars->find(arg->GetName())) {
      params2.add(*arg) ;
    } else {
      coutW(InputArguments) << "RooFitResult::createHessePdf(" << GetName() << ") WARNING input variable "
             << arg->GetName() << " was not a floating parameters in fit result and is ignored" << std::endl ;
    }
  }

  // Need to order params in vector in same order as in covariance matrix
  RooArgList params3 ;
  for(RooAbsArg* arg : *_finalPars) {
    if (params2.find(arg->GetName())) {
      params3.add(*arg) ;
    }
  }


  // Handle special case of representing full covariance matrix here
  if (params3.size()==_finalPars->size()) {

    RooArgList mu ;
    for (std::size_t i=0 ; i<_finalPars->size() ; i++) {
      RooRealVar* parclone = static_cast<RooRealVar*>(_finalPars->at(i)->Clone(Form("%s_centralvalue",_finalPars->at(i)->GetName()))) ;
      parclone->setConstant(true) ;
      mu.add(*parclone) ;
    }

    string name  = Form("pdf_%s",GetName()) ;
    string title = Form("P.d.f of %s",GetTitle()) ;

    // Create p.d.f.
    RooAbsPdf* mvg = new RooMultiVarGaussian(name.c_str(),title.c_str(),params3,mu,V) ;
    mvg->addOwnedComponents(mu) ;
    return  mvg ;
  }

  //                                       -> ->
  // Handle case of conditional p.d.f. MVG(p1|p2) here

  // Find (subset) of parameters that are stored in the covariance matrix
  vector<int> map1;
  vector<int> map2;
  for (std::size_t i=0 ; i<_finalPars->size() ; i++) {
    if (params3.find(_finalPars->at(i)->GetName())) {
      map1.push_back(i) ;
    } else {
      map2.push_back(i) ;
    }
  }

  // Rearrange matrix in block form with 'params' first and 'others' last
  // (preserving relative order)
  TMatrixDSym S11;
  TMatrixDSym S22;
  TMatrixD S12;
  TMatrixD S21;
  RooMultiVarGaussian::blockDecompose(V,map1,map2,S11,S12,S21,S22) ;

  // Calculate offset vectors mu1 and mu2
  RooArgList mu1 ;
  for (UInt_t i=0 ; i<map1.size() ; i++) {
    RooRealVar* parclone = static_cast<RooRealVar*>(_finalPars->at(map1[i])->Clone(Form("%s_centralvalue",_finalPars->at(map1[i])->GetName()))) ;
    parclone->setConstant(true) ;
    mu1.add(*parclone) ;
  }

  // Constructed conditional matrix form         -1
  // F(X1|X2) --> CovI --> S22bar = S11 - S12 S22  S21

  // Do eigenvalue decomposition
  TMatrixD S22Inv(TMatrixD::kInverted,S22) ;
  TMatrixD S22bar =  S11 - S12 * (S22Inv * S21) ;

  // Convert explicitly to symmetric form
  TMatrixDSym Vred(S22bar.GetNcols()) ;
  for (int i=0 ; i<Vred.GetNcols() ; i++) {
    for (int j=i ; j<Vred.GetNcols() ; j++) {
      Vred(i,j) = (S22bar(i,j) + S22bar(j,i))/2 ;
      Vred(j,i) = Vred(i,j) ;
    }
  }
  string name  = Form("pdf_%s",GetName()) ;
  string title = Form("P.d.f of %s",GetTitle()) ;

  // Create p.d.f.
  RooAbsPdf* ret =  new RooMultiVarGaussian(name.c_str(),title.c_str(),params3,mu1,Vred) ;
  ret->addOwnedComponents(mu1) ;
  return ret ;
}



////////////////////////////////////////////////////////////////////////////////
/// Change name of RooFitResult object

void RooFitResult::SetName(const char *name)
{
  if (_dir) _dir->GetList()->Remove(this);
  TNamed::SetName(name) ;
  if (_dir) _dir->GetList()->Add(this);
}


////////////////////////////////////////////////////////////////////////////////
/// Change name and title of RooFitResult object

void RooFitResult::SetNameTitle(const char *name, const char* title)
{
  if (_dir) _dir->GetList()->Remove(this);
  TNamed::SetNameTitle(name,title) ;
  if (_dir) _dir->GetList()->Add(this);
}


////////////////////////////////////////////////////////////////////////////////
/// Print name of fit result

void RooFitResult::printName(ostream& os) const
{
  os << GetName() ;
}


////////////////////////////////////////////////////////////////////////////////
/// Print title of fit result

void RooFitResult::printTitle(ostream& os) const
{
  os << GetTitle() ;
}


////////////////////////////////////////////////////////////////////////////////
/// Print class name of fit result

void RooFitResult::printClassName(ostream& os) const
{
  os << ClassName() ;
}


////////////////////////////////////////////////////////////////////////////////
/// Print arguments of fit result, i.e. the parameters of the fit

void RooFitResult::printArgs(ostream& os) const
{
  os << "[constPars=" << *_constPars << ",floatPars=" << *_finalPars << "]" ;
}



////////////////////////////////////////////////////////////////////////////////
/// Print the value of the fit result, i.e.g the status, minimized FCN, edm and covariance quality code

void RooFitResult::printValue(ostream& os) const
{
  os << "(status=" << _status << ",FCNmin=" << _minNLL << ",EDM=" << _edm << ",covQual=" << _covQual << ")" ;
}


////////////////////////////////////////////////////////////////////////////////
/// Configure default contents to be printed

Int_t RooFitResult::defaultPrintContents(Option_t* /*opt*/) const
{
  return kName|kClassName|kArgs|kValue ;
}


////////////////////////////////////////////////////////////////////////////////
/// Configure mapping of Print() arguments to RooPrintable print styles

RooPrintable::StyleOption RooFitResult::defaultPrintStyle(Option_t* opt) const
{
  if (!opt || strlen(opt)==0) {
    return kStandard ;
  }
  return RooPrintable::defaultPrintStyle(opt) ;
}


////////////////////////////////////////////////////////////////////////////////
/// Stream an object of class RooFitResult.

void RooFitResult::Streamer(TBuffer &R__b)
{
  if (R__b.IsReading()) {
    UInt_t R__s;
    UInt_t R__c;
    Version_t R__v = R__b.ReadVersion(&R__s, &R__c);
    if (R__v>3) {
      R__b.ReadClassBuffer(RooFitResult::Class(),this,R__v,R__s,R__c);
      RooAbsArg::ioStreamerPass2Finalize();
      _corrMatrix.SetOwner();
    } else {
      // backward compatibitily streaming
      TNamed::Streamer(R__b);
      RooPrintable::Streamer(R__b);
      RooDirItem::Streamer(R__b);
      R__b >> _status;
      R__b >> _covQual;
      R__b >> _numBadNLL;
      R__b >> _minNLL;
      R__b >> _edm;
      R__b >> _constPars;
      R__b >> _initPars;
      R__b >> _finalPars;
      R__b >> _globalCorr;
      _corrMatrix.Streamer(R__b);
      R__b.CheckByteCount(R__s, R__c, RooFitResult::IsA());

      // Now fill new-style covariance and correlation matrix information
      // from legacy form
      _CM = new TMatrixDSym(_finalPars->size()) ;
      _VM = new TMatrixDSym(_CM->GetNcols()) ;
      _GC = new TVectorD(_CM->GetNcols()) ;

      for (unsigned int i = 0; i < (unsigned int)_CM->GetNcols() ; ++i) {

   // Find the next global correlation slot to fill, skipping fixed parameters
   auto& gcVal = static_cast<RooRealVar&>((*_globalCorr)[i]);
   (*_GC)(i) = gcVal.getVal() ;

   // Fill a row of the correlation matrix
   auto corrMatrixCol = static_cast<RooArgList const&>(*_corrMatrix.At(i));
   for (unsigned int it = 0; it < (unsigned int)_CM->GetNcols() ; ++it) {
     auto& cVal = static_cast<RooRealVar&>(corrMatrixCol[it]);
     double value = cVal.getVal() ;
     (*_CM)(it,i) = value ;
     (*_CM)(i,it) = value;
     (*_VM)(it,i) = value*(static_cast<RooRealVar*>(_finalPars->at(i)))->getError()*(static_cast<RooRealVar*>(_finalPars->at(it)))->getError() ;
     (*_VM)(i,it) = (*_VM)(it,i) ;
   }
      }
    }

   } else {
      R__b.WriteClassBuffer(RooFitResult::Class(),this);
   }
}

