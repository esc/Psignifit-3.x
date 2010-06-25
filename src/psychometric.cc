/*
 *   See COPYING file distributed along with the psignifit package for
 *   the copyright and license terms
 */
#include "psychometric.h"
#include "special.h"
#include "linalg.h"

// #ifdef DEBUG_PSYCHOMETRIC
#include <iostream>
// #endif

PsiPsychometric::PsiPsychometric (
	int nAFC,
	PsiCore * core,
	PsiSigmoid * sigmoid
	) : Nalternatives(nAFC), guessingrate(1./nAFC), gammaislambda(false), priors( getNparams() )
{
	unsigned int k;
	Core = core->clone();
	Sigmoid = sigmoid->clone();
	for (k=0; k<priors.size(); k++)
		priors[k] = new PsiPrior;
}

PsiPsychometric::~PsiPsychometric ( void )
{
	unsigned int k;
	delete Core;
	delete Sigmoid;
	for (k=0; k<priors.size(); k++) {
		delete priors[k];
	}
}

double PsiPsychometric::evaluate ( double x, const std::vector<double>& prm ) const
{
	double gamma(guessingrate);
	if (Nalternatives==1) {// Here we talk about a yes/no task
		if (gammaislambda)
			gamma = prm[2];
		else
			gamma = prm[3];
	}

#ifdef DEBUG_PSYCHOMETRIC
	std::cerr << "Evaluating psychometric function. Parameters: \n";
	std::cerr << "alpha = " << prm[0] << "\nbeta = " << prm[1] << "\nlambda = " << prm[2] << "\ngamma = " << gamma << "\n";
	std::cerr << "Value of sigmoid: " << Sigmoid->f(Core->g(x,prm)) << "\n";
#endif

	return gamma + (1-gamma-prm[2]) * Sigmoid->f(Core->g(x,prm));
}

double PsiPsychometric::negllikeli ( const std::vector<double>& prm, const PsiData* data ) const
{
	unsigned int i;
	int n,k;
	double l(0);
	double x,p,lognoverk;

	for (i=0; i<data->getNblocks(); i++)
	{
		n = data->getNtrials(i);
		k = data->getNcorrect(i);
		x = data->getIntensity(i);
		lognoverk = data->getNoverK(i);
		p = evaluate(x, prm);
		l -= lognoverk;
		if (p>0)
			l -= k*log(p);
		else
			l += 1e10;
		if (p<1)
			l -= (n-k)*log(1-p);
		else
			l += 1e10;
	}

	return l;
}

double PsiPsychometric::leastfavourable ( const std::vector<double>& prm, const PsiData* data, double cut, bool threshold ) const
{
	if (!threshold) throw NotImplementedError();  // So far we only have this for the threshold

	std::vector<double> delta (prm.size(),0), du(prm.size(),0);
	Matrix * I;
	double ythres;
	double rz,nz,xz,pz,fac1;
	double l_LF(0);
	double s;
	unsigned int i,z;

	// Fill u
	ythres = Sigmoid->inv(cut);
	du[0] = Core->dinv(ythres,prm,0);
	du[1] = Core->dinv(ythres,prm,1);

	// Determine 2nd derivative
	I = ddnegllikeli ( prm, data );

	// Now we have to solve I*delta = du for delta
	try {
		delta = I->solve ( du );
	} catch (std::string){
		// In this case, the matrix is numerically singular
		// Thats bad. We simply
		delete I;
		return 0;
		// in that case
	}

	// I is not needed anymore
	delete I;

	// Normalize the result
	s = 0;
	for (i=0; i<prm.size(); i++)
		s += delta[i]*delta[i];
	s = sqrt(s);
	for (i=0; i<prm.size(); i++)
	    delta[i] /= s;

	// The result has to be multiplied by the gradient of the likelihood
	for (z=0; z<data->getNblocks(); z++) {
		rz = data->getNcorrect(z);
		nz = data->getNtrials(z);
		xz = data->getIntensity(z);
		pz = evaluate(xz,prm);
		fac1 = rz/pz - (nz-rz)/(1-pz);
		for (i=0; i<2; i++)
			l_LF += delta[i] * fac1 * Sigmoid->df(Core->g(xz,prm)) * Core->dg(xz,prm,i);
	
		for (i=2; i<prm.size(); i++)
			l_LF += delta[i] * fac1 * ( (i==2 ? 1 : 0) - Sigmoid->f(Core->g(xz,prm)) );
	}

	// If l_LF is nan, return 0
	if (l_LF!=l_LF)
		return 0;

	return l_LF;
}

Matrix * PsiPsychometric::ddnegllikeli ( const std::vector<double>& prm, const PsiData* data ) const
{
	Matrix * I = new Matrix ( prm.size(), prm.size() );

	double rz,nz,pz,xz,fac1,fac2;
	unsigned int z,i,j;

	// Fill I
	for (z=0; z<data->getNblocks(); z++) {
		nz = data->getNtrials(z);
		xz = data->getIntensity(z);
		pz = evaluate(xz,prm);
		// rz = data->getNcorrect(z);
		rz = pz*nz;     // expected Fisher Information matrix
		fac1 = rz/pz - (nz-rz)/(1-pz);
		fac2 = rz/(pz*pz) + (nz-rz)/((1-pz)*(1-pz));

		// These parts must be determined
		for (i=0; i<2; i++) {
			for (j=i; j<2; j++) {
				(*I)(i,j) += fac1 * (1-guessingrate-prm[2]) * (Sigmoid->ddf(Core->g(xz,prm)) * Core->dg(xz,prm,i) * Core->dg(xz,prm,j) + Sigmoid->df(Core->g(xz,prm)) * Core->ddg(xz,prm,i,j));
				(*I)(i,j) -= fac2 * (1-guessingrate-prm[2]) * (1-guessingrate-prm[2]) * pow(Sigmoid->df(Core->g(xz,prm)),2) * Core->dg(xz,prm,i) * Core->dg(xz,prm,j);
			}
			for (j=2; j<prm.size(); j++) {
				(*I)(i,j) -= fac1 * Sigmoid->df(Core->g(xz,prm)) * Core->dg(xz,prm,i);
				(*I)(i,j) -= fac2 * (1-guessingrate-prm[2]) * Sigmoid->df(Core->g(xz,prm)) * Core->dg(xz,prm,i) * ( (j==2 ? 0 : 1) - Sigmoid->f(Core->g(xz,prm)) );
			}
		}
		for ( i=2; i<prm.size(); i++ ) {
			for ( j=2; j<prm.size(); j++ ) {
				(*I)(i,j) -= fac2 * ( (j==2 ? 0 : 1) - Sigmoid->f(Core->g(xz,prm)) ) * ( (i==2 ? 0 : 1 ) - Sigmoid->f(Core->g(xz,prm)) );
			}
		}
	}

	// The remaining parts of I can be copied
	for (i=1; i<prm.size(); i++)
		for (j=0; j<i; j++)
			(*I)(i,j) = (*I)(j,i);

	I->scale(-1);

	return I;
}

std::vector<double> PsiPsychometric::dnegllikeli ( const std::vector<double>& prm, const PsiData* data ) const
{
	std::vector<double> out (prm.size());
	double rz,xz,pz,nz,fac1;
	unsigned int z,i;

	for (z=0; z<data->getNblocks(); z++) {
		rz = data->getNcorrect(z);
		nz = data->getNtrials(z);
		xz = data->getIntensity(z);
		pz = evaluate(xz,prm);
		fac1 = rz/pz - (nz-rz)/(1-pz);
		for (i=0; i<2; i++)
			out[i] -= fac1 * (1-guessingrate-prm[2]) * Sigmoid->df(Core->g(xz,prm)) * Core->dg(xz,prm,i);
	
		for (i=2; i<prm.size(); i++)
			out[i] -= fac1 * ( (i==2 ? 0 : 1) - Sigmoid->f(Core->g(xz,prm)) );
	}

	return out;
}

double PsiPsychometric::deviance ( const std::vector<double>& prm, const PsiData* data ) const
{
	unsigned int i;
	int n;
	double D(0);
	double x,y,p;

	for ( i=0; i<data->getNblocks(); i++ )
	{
		n = data->getNtrials(i);
		y = data->getPcorrect(i);
		x = data->getIntensity(i);
		p = evaluate( x, prm );
		if (y>0)
			D += n*y*log(y/p);
		if (y<1)
			D += n*(1-y)*log((1-y)/(1-p));
	}
	D *= 2;
	return D;
}

void PsiPsychometric::setPrior ( unsigned int index, PsiPrior* prior ) throw(BadArgumentError)
{
	if ( index >= priors.size() ) {
		throw BadArgumentError ( "Trying to set a prior for a nonexistent parameter" );
	}
	delete priors[index];
	priors[index] = prior->clone();
}

double PsiPsychometric::neglpost ( const std::vector<double>& prm, const PsiData* data ) const
{
	unsigned int i;
	double l;
	l = negllikeli( prm, data);

	for (i=0; i<getNparams(); i++) {
		priors[i]->pdf ( 0.5 );
		l -= log( priors[i]->pdf(prm[i]) );
	}

	return l;
}

std::vector<double> PsiPsychometric::getStart ( const PsiData* data ) const
{
	unsigned int i;
	double a,b,a0,b0,abest,bbest;
	double alpha,beta,alpha0,beta0,minpost,post;
	std::vector<double> x (data->getIntensities());
	std::vector<double> p (data->getPcorrect());
	double minp(1000), maxp(-1000);
	double meanx(0), meanp(0);
	double varx(0),covxp(0);
	std::vector<int> relevant (data->nonasymptotic());
	double alphamin = 1e10;
	double alphamax = -1e10;
	double pmax=0;
	double pmin=1;
	double pp;
	double imax (0),imin (0);
	if (relevant.size()==1) {
		// This is not the best solution
		for ( i=0; i<data->getNblocks(); i++ ) {
			a = data->getIntensity(i);
			pp = data->getPcorrect(i);
			if (a>alphamax) alphamax=a;
			if (a<alphamin) alphamin=a;
			if (pp>pmax) { pmax=pp; imax=i; }
			if (pp<pmin) { pmin=pp; imin=i; }
		}
	} else {
		// This is better
		for ( i=0; i<relevant.size(); i++ ) {
			a = data->getIntensity(relevant[i]);
			pp = data->getPcorrect(relevant[i]);
			if (a>alphamax) alphamax=a;
			if (a<alphamin) alphamin=a;
			if (pp>pmax) { pmax=pp; imax=relevant[i]; }
			if (pp<pmin) { pmin=pp; imin=relevant[i]; }
		}
	}
	double betamax = (data->getIntensity(imax)-data->getIntensity(imin));
	double betamin = (data->getIntensity(imax)>data->getIntensity(imin) ? 1 : -1 ) * 0.01;

	// First find the best values using logistic regression
	// Scale the data to the interval (0,1)
	for (i=0; i<x.size(); i++)
		if (minp>p[i]) minp = p[i];
#ifdef DEBUG_PSYCHOMETRIC
	std::cerr << "minp="<<minp << std::endl;
#endif

	if (minp==0) {
		for (i=0; i<x.size(); i++)
			p[i] += 0.0001;
	} else {
		for (i=0; i<x.size(); i++)
			p[i] -= 0.999*minp;
	}

	for (i=0; i<x.size(); i++)
		if (maxp<p[i]) maxp = p[i];
#ifdef DEBUG_PSYCHOMETRIC
	std::cerr << "maxp="<<maxp << std::endl;
#endif

	for (i=0; i<x.size(); i++)
		p[i] /= 1.0001*maxp;

	// Apply logit
	for (i=0; i<x.size(); i++) {
#ifdef DEBUG_PSYCHOMETRIC
		std::cerr << "p["<<i<<"] = "<<p[i]<<"\t";
#endif
		p[i] = log(p[i]/(1-p[i]));
#ifdef DEBUG_PSYCHOMETRIC
		std::cerr << "lp["<<i<<"] = "<<p[i]<<"\n";
#endif
	}

	// Determine averages
	for (i=0; i<x.size(); i++) {
		meanx += x[i];
		meanp += p[i];
	}
	meanx /= x.size();
	meanp /= x.size();

	// Compute covariances
	for (i=0; i<x.size(); i++) {
		varx += (x[i]-meanx)*(x[i]-meanx);
		covxp += (x[i]-meanx)*(p[i]-meanp);
	}

	b0 = covxp/varx;
	a0 = meanp - meanx*b0;
	
	alpha0 = a0/b0;
	beta0  = 1./b0;
	abest = a0;
	bbest = b0;

	std::vector<double> out;
	out = Core->transform ( getNparams(), abest, bbest );
	if (Nalternatives==1 && !gammaislambda) {
		out[2] = 0.02;
		out[3] = 0.02;
	} else {
		out[2] = 0.02;
	}
	minpost = neglpost ( out, data );


	// Now perform a little grid search to maybe improve the parameters (and thus avoid the worst local minima)
	for ( beta=betamin; beta<=betamax; beta+= (betamax-betamin)/10. ) {
		for ( alpha=alphamin; alpha<=alphamax; alpha+= (alphamax-alphamin)/10. ) {
			// a = alpha/beta - beta0;
			a = -alpha/beta;
			b = 1./beta;
			out = Core->transform( getNparams(), a,b);
			if (Nalternatives==1 && !gammaislambda) {
				out[2] = 0.02;
				out[3] = .02;
			} else {
				out[2] = 0.02;
			}
			post = neglpost ( out, data );
			if ( post < minpost ) {
				abest = a;
				bbest = b;
				minpost = post;
			}
		}
	}

	// Store the super best parameters in the end
	out = Core->transform ( getNparams(), abest, bbest );
	if (Nalternatives==1 && !gammaislambda) {
		out[2] = 0.02;
		out[3] = 0.02;
	} else {
		out[2] = 0.02;
	}

	return out;
}

std::vector<double> PsiPsychometric::getDevianceResiduals ( const std::vector<double>& prm, const PsiData* data ) const
{
	unsigned int i;
	int n;
	double x,y,p;
	std::vector<double> out (data->getNblocks());

	for ( i=0; i<data->getNblocks(); i++ )
	{
		n = data->getNtrials(i);
		y = data->getPcorrect(i);
		x = data->getIntensity(i);
		p = evaluate(x,prm);
		out[i] = 0;
		if (y>0)
			out[i] += n*y*log(y/p);
		if (y<1)
			out[i] += n*(1-y)*log((1-y)/(1-p));
		out[i] = (y>p?1:-1) * sqrt(2*out[i]);
	}

	return out;
}

double PsiPsychometric::getRpd ( const std::vector<double>& devianceresiduals, const std::vector<double>& prm, const PsiData* data ) const {
	int k,N(data->getNblocks());
	double Ed(0),Ep(0),vard(0),varp(0),R(0);
	std::vector<double> p ( N );

	// Evaluate p values in advance
	for ( k=0; k<N; k++ ) {
		p[k] = evaluate(data->getIntensity(k),prm);
	}

	// Calculate averages
	for ( k=0; k<N; k++ ) {
		Ed += devianceresiduals[k];
		Ep += p[k];
	}
	Ed /= N;
	Ep /= N;

	// Calculate unnormalized variances and covariances
	for ( k=0; k<N; k++ ) {
		vard += pow(devianceresiduals[k]-Ed,2);
		varp += pow(p[k]-Ep,2);
		R    += (devianceresiduals[k]-Ed)*(p[k]-Ep);
	}

	// Normalize and return
	R /= sqrt(vard);
	R /= sqrt(varp);

	return R;
}

double PsiPsychometric::getRkd ( const std::vector<double>& devianceresiduals, const PsiData* data ) const
{
	int i,k;
	double Ed(0), Ek(0), vard(0), vark(0), R(0);
	std::vector<int> ofinterest ( data->nonasymptotic() );
	int M ( ofinterest.size() );

	// Calculate averages
	for ( k=0; k<M; k++ ) {
		i = ofinterest[k];
		Ed += devianceresiduals[i];
		Ek += k;  // This should be i in my opinion, but it fits the old psignifit only if it's k
	}
	Ed /= M;
	Ek /= M;

	// Calculate unnormalized variances and covariances
	for ( k=0; k<M; k++ ) {
		i = ofinterest[k];
		vard += pow(devianceresiduals[i]-Ed,2);
		vark += pow(k-Ek,2);         // Here k should be replaced by i in my opinion
		R    += (devianceresiduals[i]-Ed)*(k-Ek);  // here again, k should be replaced by i in my opinion
	}

	// Normalize and return
	R /= sqrt(vard);
	R /= sqrt(vark);

	return R;
}

double PsiPsychometric::dllikeli ( std::vector<double> prm, const PsiData* data, unsigned int i ) const
{
	int k, Nblocks(data->getNblocks());
	double rz,pz,nz,xz,dl(0);
	double guess(1./Nalternatives);
	if (Nalternatives==1) {// Here we talk about a yes/no task
		if (gammaislambda)
			guess = prm[2];
		else
			guess = prm[3];
	}

	for (k=0; k<Nblocks; k++) {
		rz = data->getNcorrect ( k );
		nz = data->getNtrials  ( k );
		xz = data->getIntensity( k );
		pz = evaluate ( xz, prm );
		switch (i) {
			case 0: case 1:
				dl += (rz/pz - (nz-rz)/(1-pz)) * (1-guess-prm[2]) * Sigmoid->df ( Core->g ( xz, prm ) ) * Core->dg ( xz, prm, i );
				break;
			case 2:
				dl -= (rz/pz - (nz-rz)/(1-pz)) * Sigmoid->f ( Core->g ( xz, prm ) );
				break;
			case 3:
				if (Nalternatives==1) // gamma is a free parameter
					dl += (rz/pz - (nz-rz)/(1-pz)) * (1 - Sigmoid->f ( Core->g ( xz, prm ) ));
				break;
		}
	}

	return dl;
}

double PsiPsychometric::dlposteri ( std::vector<double> prm, const PsiData* data, unsigned int i ) const
{
	if ( i < getNparams() )
		return dllikeli ( prm, data, i ) + priors[i]->dpdf(prm[i]);
	else
		return 0;
}

/******************************** Outlier model *****************************************/

double OutlierModel::getp ( const std::vector<double>& prm ) const
{
	if ( getNalternatives()<2 )
		return prm[4];
	else
		return prm[3];
}

double OutlierModel::negllikeli ( const std::vector<double>& prm, const PsiData* data ) const
{
	if ( getNalternatives() != data->getNalternatives() )
		throw BadArgumentError();

	std::vector<double> x ( data->getNblocks()-1 );
	std::vector<int>    k ( data->getNblocks()-1 );
	std::vector<int>    n ( data->getNblocks()-1 );
	unsigned int i,j(0);
	double ll;
	double p;


	for ( i=0; i<data->getNblocks(); i++ ) {
		if (i!=jout) {
			x[j] = data->getIntensity(i);
			k[j] = data->getNcorrect(i);
			n[j] = data->getNtrials(i);
			j++;
		}
	}

	PsiData * localdata = new PsiData ( x, n, k, data->getNalternatives() );
	// for (i=0; i<localdata->getNblocks(); i++) std::cerr << localdata->getIntensity(i) << " " << localdata->getNcorrect(i) << " " << localdata->getNtrials(i) << "\n";

	p = getp( prm );

	ll = PsiPsychometric::negllikeli ( prm, localdata );
	ll -= data->getNoverK(jout);
	if (p>0) ll -= data->getNcorrect(jout) * log(p);
	if (p<1) ll -= ( data->getNtrials(jout)-data->getNcorrect (jout) ) * log ( 1-p );

	delete localdata;

	return ll;
}

double OutlierModel::deviance ( const std::vector<double>& prm, const PsiData* data ) const
{
	unsigned int i;
	int n;
	double D(0);
	double x,y,p;

	for ( i=0; i<data->getNblocks(); i++ )
	{
		n = data->getNtrials(i);
		y = data->getPcorrect(i);
		x = data->getIntensity(i);
		if (i==jout)
			p = getp( prm );
		else
			p = evaluate( x, prm );
		if (y>0)
			D += n*y*log(y/p);
		if (y<1)
			D += n*(1-y)*log((1-y)/(1-p));
	}

	if (D!=D)
		std::cerr << p << "\n";

	D *= 2;
	return D;
}

double OutlierModel::neglpost ( const std::vector<double>& prm, const PsiData* data ) const
{
	unsigned int i;
	double l;
	l = negllikeli( prm, data);

	for (i=0; i<getNparams()-1; i++) {
		l -= log( evalPrior(i, prm[i]) );
	}

	if ( getp(prm)<0 || getp(prm)> 1 )
		l += 1e10;

	return l;
}
