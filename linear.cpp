#include <iostream>
#include <fstream>
#include <boost/math/distributions.hpp>
#include <boost/array.hpp>
#include <boost/numeric/odeint.hpp>
#include <random>
#include <vector>
#include <Eigen/Dense>
#include <Eigen/Core>
#include <unsupported/Eigen/MatrixFunctions>
#include <cmath>
#include <chrono>
#include <omp.h>
#include <Eigen/StdVector>
#include <boost/numeric/odeint/external/openmp/openmp.hpp>

#define N_SPECIES 3
#define N_DIM 5 // dim of PSO hypercube

using Eigen::MatrixXd;
using Eigen::VectorXd;
using namespace std;
using namespace boost;
using namespace boost::math;
using namespace boost::numeric::odeint;


struct Multi_Normal_Random_Variable
{
    Multi_Normal_Random_Variable(Eigen::MatrixXd const& covar)
        : Multi_Normal_Random_Variable(Eigen::VectorXd::Zero(covar.rows()), covar)
    {}

    Multi_Normal_Random_Variable(Eigen::VectorXd const& mean, Eigen::MatrixXd const& covar)
        : mean(mean)
    {
        Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> eigenSolver(covar);
        transform = eigenSolver.eigenvectors() * eigenSolver.eigenvalues().cwiseSqrt().asDiagonal();
    }

    Eigen::VectorXd mean;
    Eigen::MatrixXd transform;

    Eigen::VectorXd operator()() const
    {
        static std::mt19937 gen{ std::random_device{}() }; //std::random_device {} ()
        static std::normal_distribution<> dist;

        return mean + transform * Eigen::VectorXd{ mean.size() }.unaryExpr([&](auto x) { return dist(gen); });
    }
};

struct K
{
    VectorXd k;
};

VectorXd moment_vector(const MatrixXd &sample, int nMoments){
    VectorXd moments(nMoments);
    VectorXd mu = sample.colwise().mean();
    VectorXd variances(sample.cols());

    int nVar = sample.cols();// check to make sure if we are using variances to compute means, variances, etc. 
    if(nMoments < sample.cols()){
        nVar = 0;
    }
    // dont forget to subtract mu.
    for(int c = 0; c < nVar; c++){
        variances(c) = (sample.col(c).array()).square().sum() / ((double) sample.col(c).array().size() - 1);
    }

    int nCross = nMoments - 2*sample.cols();
    VectorXd covariances(0);
    if(nCross > 0){
        int n = 0;
        covariances.conservativeResize(nCross);
        for (int i = 0; i < sample.cols(); i++) {
            for (int j = i + 1; j < sample.cols(); j++) {
                covariances(n) = ((sample.col(i).array() * sample.col(j).array()).sum())/( sample.rows() - 1);
                n++;
            }
        }
    }

    // concatenate all moment vectors needed.
    for(int i = 0; i < nMoments; i++){
        if(i < sample.cols()){
            moments(i) = mu(i);
        }else if (i >= sample.cols() && i < 2 * sample.cols()){
            moments(i) = variances(i - sample.cols());
        }else if (i >= 2 * sample.cols()){
            moments(i) = covariances(i - (2 * sample.cols()));
        }
    }
    return moments;

}


/* placeholder function -> will generalize for greater proteins if we ever get a better matrix. */
MatrixXd evolutionMatrix(VectorXd &k, double tf, int nSpecies){
    MatrixXd M(nSpecies, nSpecies);
	M << -k(2), k(2), 0,
		k(1), -k(1) - k(4), k(4),
		k(3), k(0), -k(0) - k(3);

	MatrixXd MT(nSpecies, nSpecies);
	MT = tf * M.transpose();
   
	MatrixXd EMT(nSpecies, nSpecies);
	EMT = MT.exp();

    return EMT;
}


VectorXd comp_vel_vec(const VectorXd& posK, int seed, double epsi, double nan, int hone) {
    VectorXd rPoint;
    rPoint = posK;
    std::random_device rand_dev;
    std::mt19937 generator(rand_dev());
    vector<int> rand;
    uniform_real_distribution<double> unifDist(0.0, 1.0);
    for (int i = 0; i < N_DIM; i++) {
        rand.push_back(i);
    }
    shuffle(rand.begin(), rand.end(), generator); // shuffle indices as well as possible. 
    int ncomp = rand.at(0);
    VectorXd wcomp(ncomp);
    shuffle(rand.begin(), rand.end(), generator);
    for (int i = 0; i < ncomp; i++) {
        wcomp(i) = rand.at(i);
    }
    
    for (int smart = 0; smart < ncomp; smart++) {
        int px = wcomp(smart);
        double pos = rPoint(px);
        if (pos > 1.0 - nan) {
            cout << "overflow!" << endl;
            pos -= epsi;
        }else if (pos < nan) {
            cout << "underflow!"<< pos << endl;
            pos += epsi;
            cout << "pos" << posK.transpose() << endl; 
        }
        double alpha = hone * pos; // Component specific
        double beta = hone - alpha; // pos specific
    // cout << "alpha:" << alpha << "beta:" << beta << endl;
        std::gamma_distribution<double> aDist(alpha, 1); // beta distribution consisting of gamma distributions
        std::gamma_distribution<double> bDist(beta, 1);

        double x = aDist(generator);
        double y = bDist(generator);

        rPoint(smart) = (x / (x + y)); 
    }
    
    return rPoint;
}
MatrixXd calculate_omega_weight_matrix(const MatrixXd &sample, const VectorXd &mu){
    MatrixXd inv = MatrixXd::Zero(mu.size(), mu.size());
    VectorXd X = VectorXd::Zero(mu.size());
    
    for(int s = 0; s < sample.rows(); s++){
        int upperDiag = 2 * N_SPECIES;
        for(int row = 0; row < N_SPECIES; row++){
            X(row) = sample(s, row); 
            for(int col = row; col < N_SPECIES; col++){
                if(row == col){
                    X(N_SPECIES + row) = sample(s, row) * sample(s, col);
                }else{
                    X(upperDiag) = sample(s,row) * sample(s,col);
                    upperDiag++;
                }
            }
        }
        for(int i = 0; i < mu.size(); i++){
            for(int j = 0; j < mu.size(); j++){
                inv(i,j) += (X(i) - mu(i)) * (X(j) - mu(j));
            }
        }
    }
    inv /= sample.rows();
    inv = inv.completeOrthogonalDecomposition().pseudoInverse();
    return inv;
}
double calculate_cf1(const VectorXd& trueVec, const VectorXd& estVec) {
    double cost = 0;
    VectorXd diff(trueVec.size());
    diff = trueVec - estVec;
    cost = diff.transpose() * diff.transpose().transpose();
    return cost;
}
double calculate_cf2(const VectorXd& trueVec, const  VectorXd& estVec, const MatrixXd& w) {
    double cost = 0;
    VectorXd diff(trueVec.size());
    diff = trueVec - estVec;
    cost = diff.transpose() * w * (diff.transpose()).transpose();
    return cost;
}
string removeWhiteSpace(string current)
{
  string myNewString = "";
  string temp = "x";
  for (char c : current)
  {
    if (temp.back() != ' ' && c == ' ')
    {
      myNewString.push_back(' ');
    }
    temp.push_back(c);
    if (c != ' ')
    {
      myNewString.push_back(c);
    }
  }
  return myNewString;
}

string findDouble(string line, int startPos) {
    string doble;
    int i = startPos;
    int wDist = 0;
    while (i < line.length() && !isspace(line.at(i)) && line.at(i) != '\t') {
        i++;
        wDist++;
    }
    doble = line.substr(startPos, wDist);

    return doble;
}
MatrixXd readIntoMatrix(ifstream& in, int rows, int cols) {
    MatrixXd mat(rows, cols);
    // use first row to determine how many columns to read.
    for (int i = 0; i < rows; i++) {
        string line;
        if (in.is_open()) {
            getline(in, line);
            line = removeWhiteSpace(line);
            int wordPos = 0;
            for (int j = 0; j < cols; j++) {
                string subs = findDouble(line, wordPos);
                mat(i, j) = stod(subs);
                wordPos += subs.length() + 1;
            }
        }
        else {
            cout << "Error: File Closed!" << endl;
        }

    }
    return mat;
}
MatrixXd customWtMat(const MatrixXd& Yt, const MatrixXd& Xt, int nMoments, int N, const VectorXd& subCol){
    
    bool useInverse = true;
    /* first moment differences */
    MatrixXd fmdiffs = Yt - Xt; 
    /* second moment difference computations - @todo make it variable later */
    MatrixXd smdiffs(N, N_SPECIES);
    for(int i = 0; i < N_SPECIES; i++){
        smdiffs.col(i) = (Yt.col(i).array() * Yt.col(i).array()) - (Xt.col(i).array() * Xt.col(i).array());
    }

   
    int nCross = nMoments - 2 * N_SPECIES;
    if (nCross < 0){
        nCross = 0;
    }
    MatrixXd cpDiff(N, nCross);
    
    /* cross differences */
    if(nCross > 0){
        int upperDiag = 0;
        for(int i = 0; i < N_SPECIES; i++){
            for(int j = i + 1; j < N_SPECIES; j++){
                cpDiff.col(upperDiag) = (Yt.col(i).array() * Yt.col(j).array()) - (Xt.col(i).array() * Xt.col(j).array());
                upperDiag++;
            }
        }
    }
    MatrixXd aDiff(N, nMoments);
    for(int i = 0; i < N; i++){
        for(int moment = 0; moment < nMoments; moment++){
            if(moment < N_SPECIES){
                aDiff(i, moment) = fmdiffs(i, moment);
            }else if (moment >= N_SPECIES && moment < 2 * N_SPECIES){
                aDiff(i, moment) = smdiffs(i, moment - N_SPECIES);
            }else if (moment >= 2 * N_SPECIES){
                aDiff(i, moment) = cpDiff(i, moment - (2 * N_SPECIES));
            }
        }
    }
    double cost = 0;
    VectorXd means = aDiff.colwise().mean();
    VectorXd variances(nMoments);
    for(int i = 0; i < nMoments; i++){
        variances(i) = (aDiff.col(i).array() - aDiff.col(i).array().mean()).square().sum() / ((double) aDiff.col(i).array().size() - 1);
    }
    int rank = subCol.size();
    MatrixXd wt = MatrixXd::Identity(rank, rank);
    if(!useInverse){
        for(int i = 0; i < rank; i++){
            wt(i,i) = 1 / variances(subCol(i)); // cleanup code and make it more vectorized later.
        }
    }else{
        for(int i = 0; i < aDiff.rows(); i++){
            wt += aDiff.row(i).transpose() * aDiff.row(i);
        }
        wt = (wt / aDiff.rows()).inverse();
    }


    cout << "new wt mat:" << endl << wt << endl;

    return wt;
}

void printToCsv(const MatrixXd& mat, const string& fileName){ // prints matrix to csv
    ofstream plot;
    string csvFile = fileName + ".csv";
	plot.open(csvFile);

    for(int i = 0; i < mat.rows(); i++){
        for(int j = 0; j < mat.cols(); j++){
            if(j == 0){
                plot << mat(i,j);
            }else{
                plot << "," << mat(i,j);
            }
        }
        plot << endl;
    }
    plot.close();
}

int main() {
    auto t1 = std::chrono::high_resolution_clock::now();
    /*---------------------- Setup ------------------------ */
  
    /* Variables (global) */
    double t0 = 0, tf = 3, dt = 1.0; // time variables
    int Npars = N_DIM, nSpecies = N_SPECIES;
    double squeeze = 0.500, sdbeta = 0.10; 
    double boundary = 0.001;
    /* SETUP */
    int useDiag = 0;
    int sf1 = 1;
    int sf2 = 1;
    double epsi = 0.02;
    double nan = 0.005;
    /* PSO params */
    double sfp = 3.0, sfg = 1.0, sfe = 6.0; // initial particle historical weight, global weight social, inertial
    double sfi = sfe, sfc = sfp, sfs = sfg; // below are the variables being used to reiterate weights
    double alpha = 0.2;
    int N = 5000;
    int nParts = 5000; // first part PSO
    int nSteps = 5;
    int nParts2 = 5; // second part PSO
    int nSteps2 = 5000;
    int nMoments = (N_SPECIES * (N_SPECIES + 3)) / 2; // var + mean + cov
    int hone = 4;
    //nMoments = 2*N_SPECIES; // mean + var only!
    VectorXd wmatup(4);
    wmatup << 0.15, 0.30, 0.45, 0.60;
    double uniLowBound = 0.0, uniHiBound = 1.0;
    random_device RanDev;
    mt19937 gen(RanDev());
    uniform_real_distribution<double> unifDist(uniLowBound, uniHiBound);
    
    MatrixXd weight = MatrixXd::Identity(nMoments, nMoments);

    cout << "Using two part PSO " << "Sample Size:" << N << " with:" << nMoments << " moments." << endl;
    cout << "Using Times:" << tf << endl;
    cout << "Bounds for Uniform Distribution (" << uniLowBound << "," << uniHiBound << ")"<< endl;
    cout << "Blind PSO --> nParts:" << nParts << " Nsteps:" << nSteps << endl;
    cout << "Targeted PSO --> nParts:" <<  nParts2 << " Nsteps:" << nSteps2 << endl;
    cout << "sdbeta:" << sdbeta << endl;

    MatrixXd GBMAT(0, 0); // iterations of global best vectors
    MatrixXd PBMAT(nParts, Npars + 1); // particle best matrix + 1 for cost component
    MatrixXd POSMAT(nParts, Npars); // Position matrix as it goees through it in parallel

    cout << "Reading in data!" << endl;
    /* Initial Conditions */
    int sizeFile = 25000;
    int startRow = 0;
    MatrixXd X_0_Full(sizeFile, nSpecies);
    MatrixXd Y_0_Full(sizeFile, nSpecies);
    MatrixXd X_0(N, nSpecies);
    MatrixXd Y_0(N, nSpecies);
    MatrixXd X_t(N, nSpecies);
    MatrixXd Y_t(N, nSpecies);
    ifstream X0File("knewX.0.txt");
    ifstream Y0File("knewY.0.txt");
    VectorXd YtmVec(nMoments);
    VectorXd XtmVec(nMoments);
    X_0_Full = readIntoMatrix(X0File, sizeFile, N_SPECIES);
    Y_0_Full = readIntoMatrix(Y0File, sizeFile, N_SPECIES);
    X0File.close();
    Y0File.close();
    
    cout << "Blocking Data!" << endl;
    X_0 = X_0_Full.block(startRow, 0, N, nSpecies);
    Y_0 = Y_0_Full.block(startRow, 0, N, nSpecies);
    cout << "Using starting row of data:" << startRow << " and " << N << " data pts!" << endl;
    cout << "first row X0:" << X_0.row(0) << endl;
    cout << "final row X0:" << X_0.row(N - 1) << endl << endl << endl << endl;

    /* super ugly hardcoded way of generating random initial X0 and Y0 (we'll figure out input and outputs later).*/
    std::random_device rand_dev;
	std::mt19937 generator(rand_dev());
	double mu_x = 1.47, mu_y = 1.74, mu_z = 1.99; // true means for MVN(theta)

	double var_x = 0.77, var_y = 0.99, var_z = 1.11; // true variances for MVN(theta);

	double rho_xy = 0.10, rho_xz = 0.05, rho_yz = 0.10; // true correlations for MVN

	double sigma_x = sqrt(var_x), sigma_y = sqrt(var_y), sigma_z = sqrt(var_z);

	double cov_xy = rho_xy * sigma_x * sigma_y;
	double cov_xz = rho_xz * sigma_x * sigma_z;
	double cov_yz = rho_yz * sigma_y * sigma_z;

	/* sigma matrices */
	MatrixXd sigma_12(1, 2);
	sigma_12 << cov_xz, cov_yz;
	
	MatrixXd sigma_22(2, 2);
	sigma_22 << var_x, cov_xy,
		cov_xy, var_y;
	
	MatrixXd sigma_21(2, 1);
	sigma_21 = sigma_12.transpose();

	/* conditional variances of proteins*/
	double cvar_ygx = (1 - (rho_xy * rho_xy)) * (sigma_y * sigma_y);

	double cvar_zgxy = var_z - (sigma_12 * sigma_22.inverse() * sigma_21)(0,0); // note: since matrix types are incompatible witgh doubles, we must do matrix math first, then convert to double.

    VectorXd x(N); //note the data sample x is a list of 10000 RV from normal dist
	VectorXd pa_x(N);
	VectorXd y(N);
	VectorXd pa_y(N);
	VectorXd z(N); // z big questions about how to get the data values for it. It takes in a far bigger value???
	VectorXd pa_z(N);

    std::normal_distribution<double> xNorm(mu_x, sigma_x);
    for (int i = 0; i < N; i++) {
        x(i) = (xNorm(generator));
        pa_x(i) = (exp(x(i)));
    }
    
    
    for (int i = 0; i < x.size(); i++) {
        std::normal_distribution<double> yNorm(mu_y + sigma_y * rho_xy * (x(i) - mu_x) / sigma_x, sqrt(cvar_ygx));
        y(i) = (yNorm(generator));
        pa_y(i) = (exp(y(i)));
    }
    
    /* matrix math for the z random vals. */
    MatrixXd rbind(2, N); // first calculate a 2xN rbind matrix
    for (int i = 0; i < x.size(); i++) {
        rbind(0, i) = x(i) - mu_x;
        rbind(1, i) = y(i) - mu_y;
    }
    MatrixXd zMean(1, N); // calculate the vector of means
    zMean = sigma_12 * sigma_22.inverse() * rbind;
    for (int i = 0; i < zMean.size(); i++) {
        zMean(0, i) = zMean(0, i) + mu_z;
    }
    // finally actually calculate z and pa_z vectors
    for (int i = 0; i < N; i++) {
        std::normal_distribution<double> zNorm(zMean(0,i), sqrt(cvar_zgxy));
        z(i) = (zNorm(generator));
        pa_z(i) = (exp(z(i)));
    }
    Y_0.col(0) = pa_x;
    Y_0.col(1) = pa_y;
    Y_0.col(2) = pa_z;


    for (int i = 0; i < N; i++) {
        x(i) = (xNorm(generator));
        pa_x(i) = (exp(x(i)));
    }
    
    
    for (int i = 0; i < x.size(); i++) {
        std::normal_distribution<double> yNorm(mu_y + sigma_y * rho_xy * (x(i) - mu_x) / sigma_x, sqrt(cvar_ygx));
        y(i) = (yNorm(generator));
        pa_y(i) = (exp(y(i)));
    }
    
    /* matrix math for the z random vals. */
    for (int i = 0; i < x.size(); i++) {
        rbind(0, i) = x(i) - mu_x;
        rbind(1, i) = y(i) - mu_y;
    }

    zMean = sigma_12 * sigma_22.inverse() * rbind;
    for (int i = 0; i < zMean.size(); i++) {
        zMean(0, i) = zMean(0, i) + mu_z;
    }
    // finally actually calculate z and pa_z vectors
    for (int i = 0; i < N; i++) {
        std::normal_distribution<double> zNorm(zMean(0,i), sqrt(cvar_zgxy));
        z(i) = (zNorm(generator));
        pa_z(i) = (exp(z(i)));
    }
    X_0.col(0) = pa_x;
    X_0.col(1) = pa_y;
    X_0.col(2) = pa_z;
    /*TERRIBLE IMPLEMENTATION ABOVE OF X0 AND Y0 ^ */

    /* Solve for Y_t (mu). */
    cout << "Loading in Truk!" << endl;
    struct K tru;
    tru.k = VectorXd::Zero(Npars);
    tru.k <<  0.27678200,  0.83708059, 0.44321700, 0.04244124, 0.30464502; // Bill k
    cout << "Calculating Yt!" << endl;

    Y_t = (evolutionMatrix(tru.k, tf, nSpecies) * Y_0.transpose()).transpose();
    
    YtmVec = moment_vector(Y_t, nMoments);
    cout << "did this compute?" << endl;
    /* Instantiate seedk aka global costs */
    struct K seed;
    seed.k = VectorXd::Zero(Npars); 
    for (int i = 0; i < Npars; i++) { 
        seed.k(i) = unifDist(gen);
    }

    double costSeedK = 0;
    X_t = (evolutionMatrix(seed.k, tf, nSpecies) * X_0.transpose()).transpose();
    XtmVec = moment_vector(X_t, nMoments);
    costSeedK = calculate_cf2(YtmVec, XtmVec, weight);
    cout << "seedk:"<< seed.k.transpose()<< "| cost:" << costSeedK << endl;
    cout << "XtmVec:" << XtmVec.transpose() << endl;
    cout << "YtmVec:" << YtmVec.transpose() << endl;
    double gCost = costSeedK; //initialize costs and GBMAT
    // global values
    VectorXd GBVEC = seed.k;
    
    GBMAT.conservativeResize(GBMAT.rows() + 1, Npars + 1);
    for (int i = 0; i < Npars; i++) {
        GBMAT(GBMAT.rows() - 1, i) = seed.k(i);
    }
    GBMAT(GBMAT.rows() - 1, Npars) = gCost;
    
    /* Blind PSO begins */
    cout << "PSO begins!" << endl;
    for(int step = 0; step < nSteps; step++){
        
    // #pragma omp parallel for 
        for(int particle = 0; particle < nParts; particle++){
            random_device pRanDev;
            mt19937 pGenerator(pRanDev());
            uniform_real_distribution<double> pUnifDist(uniLowBound, uniHiBound);
            /* instantiate all particle rate constants with unifDist */
            if(step == 0){
                /* temporarily assign specified k constants */
                for(int i = 0; i < Npars; i++){
                    POSMAT(particle, i) = pUnifDist(pGenerator);
                 
                }
                
                struct K pos;
                pos.k = VectorXd::Zero(Npars);
                for(int i = 0; i < Npars; i++){
                    pos.k(i) = POSMAT(particle, i);
                }
                double cost = 0;
                X_t = (evolutionMatrix(pos.k, tf, nSpecies) * X_0.transpose()).transpose();
                XtmVec = moment_vector(X_t, nMoments);
                cost = calculate_cf2(YtmVec, XtmVec, weight);
                /* instantiate PBMAT */
                for(int i = 0; i < Npars; i++){
                    PBMAT(particle, i) = POSMAT(particle, i);
                }
                PBMAT(particle, Npars) = cost; // add cost to final column
            }else{ 
                /* using new rate constants, instantiate particle best values */
                /* step into PSO */
                double w1 = sfi * pUnifDist(pGenerator)/ sf2, w2 = sfc * pUnifDist(pGenerator) / sf2, w3 = sfs * pUnifDist(pGenerator)/ sf2;
                double sumw = w1 + w2 + w3; //w1 = inertial, w2 = pbest, w3 = gbest
                w1 = w1 / sumw; w2 = w2 / sumw; w3 = w3 / sumw;
                //w1 = 0.05; w2 = 0.90; w3 = 0.05;
                struct K pos;
                pos.k = VectorXd::Zero(Npars);
                pos.k = POSMAT.row(particle);
                VectorXd rpoint = comp_vel_vec(pos.k, particle, epsi, nan, hone);
                VectorXd PBVEC(Npars);
                for(int i = 0; i < Npars; i++){
                    PBVEC(i) = PBMAT(particle, i);
                }
                pos.k = w1 * rpoint + w2 * PBVEC + w3 * GBVEC; // update position of particle
                POSMAT.row(particle) = pos.k;

                double cost = 0;
                X_t = (evolutionMatrix(pos.k, tf, nSpecies) * X_0.transpose()).transpose();
                XtmVec = moment_vector(X_t, nMoments);
                cost = calculate_cf2(YtmVec, XtmVec, weight);
               
                /* update gBest and pBest */
            //     #pragma omp critical
            //    {
                if(cost < PBMAT(particle, Npars)){ // particle best cost
                    for(int i = 0; i < Npars; i++){
                        PBMAT(particle, i) = pos.k(i);
                    }
                    PBMAT(particle, Npars) = cost;
                    if(cost < gCost){
                        gCost = cost;
                        GBVEC = pos.k;
                    }   
                }
            //   }
            }
        }
        GBMAT.conservativeResize(GBMAT.rows() + 1, Npars + 1); // Add to GBMAT after resizing
        for (int i = 0; i < Npars; i++) {GBMAT(GBMAT.rows() - 1, i) = GBVEC(i);}
        GBMAT(GBMAT.rows() - 1, Npars) = gCost;
        sfi = sfi - (sfe - sfg) / nSteps;   // reduce the inertial weight after each step 
        sfs = sfs + (sfe - sfg) / nSteps;

        // print out desired PBMAT for contour plots
        if(step == 0){
            printToCsv(PBMAT, "single_PBMAT_t30");
        }
    }

    cout << "GBMAT from blind PSO:" << endl << endl;
    cout << GBMAT << endl << endl;
    cout << "truk: " << tru.k.transpose() << endl;
    double dist = calculate_cf1(tru.k, GBVEC);
    cout << "total difference b/w truk and final GBVEC" << dist << endl << endl; // compute difference
    auto tB = std::chrono::high_resolution_clock::now();
    auto bDuration = std::chrono::duration_cast<std::chrono::seconds>(tB - t1).count();
    cout << "blind PSO FINISHED RUNNING IN " << bDuration << " s TIME!" << endl;
    /*** targeted PSO ***/
    POSMAT.conservativeResize(nParts2, Npars); // resize matrices to fit targetted PSO
    PBMAT.conservativeResize(nParts2, Npars + 1);
    VectorXd subset = VectorXd::Zero(nMoments);
    for(int i = 0; i < nMoments; i++){
        subset(i) = i;
    }

    cout << "targeted PSO has started!" << endl; 
    sfp = 3.0, sfg = 1.0, sfe = 6.0; // initial particle historical weight, global weight social, inertial
    sfi = sfe, sfc = sfp, sfs = sfg; // below are the variables being used to reiterate weights
    double nearby = sdbeta;
    VectorXd chkpts = wmatup * nSteps2;
    for(int step = 0; step < nSteps2; step++){
        if(step == 0 || step == chkpts(0) || step == chkpts(1) || step == chkpts(2) || step == chkpts(3)){ /* update wt   matrix || step == chkpts(0) || step == chkpts(1) || step == chkpts(2) || step == chkpts(3) */
            cout << "Updating Weight Matrix!" << endl;
            cout << "GBVEC AND COST:" << GBMAT.row(GBMAT.rows() - 1) << endl;
            nearby = squeeze * nearby;
            /* reinstantiate gCost */
            struct K gPos;
            gPos.k = GBVEC;
            
            double cost = 0;
            X_t = (evolutionMatrix(gPos.k, tf, nSpecies) * X_0.transpose()).transpose();
            XtmVec = moment_vector(X_t, nMoments);
            weight = customWtMat(Y_t, X_t, nMoments, N, subset);
            cost = calculate_cf2(YtmVec, XtmVec, weight);
            
            gCost = cost;
            hone += 4;
            GBMAT.conservativeResize(GBMAT.rows() + 1, Npars + 1);
            for (int i = 0; i < Npars; i++) {GBMAT(GBMAT.rows() - 1, i) = gPos.k(i);}
            GBMAT(GBMAT.rows() - 1, Npars) = gCost;
        }
    // #pragma omp parallel for 
        for(int particle = 0; particle < nParts2; particle++){
            random_device pRanDev;
            mt19937 pGenerator(pRanDev());
            uniform_real_distribution<double> pUnifDist(uniLowBound, uniHiBound);
        
            if(step == 0 || step == chkpts(0) || step == chkpts(1) || step == chkpts(2) || step == chkpts(3)){
                /* reinitialize particles around global best */
                for(int edim = 0; edim < Npars; edim++){
                    int wasflipped = 0;
                    double tmean = GBVEC(edim);
                    if (GBVEC(edim) > 0.5) {
                        tmean = 1 - GBVEC(edim);
                        wasflipped = 1;
                    }
                    double myc = (1 - tmean) / tmean;
                    double alpha = myc / ((1 + myc) * (1 + myc) * (1 + myc)*nearby*nearby);
                    double beta = myc * alpha;

                    if(alpha < nan){
                        alpha = epsi;
                    }
                    if(beta < nan){
                        beta = epsi;
                    }

                    std::gamma_distribution<double> aDist(alpha, 1);
                    std::gamma_distribution<double> bDist(beta, 1);

                    double x = aDist(pGenerator);
                    double y = bDist(pGenerator);
                    double myg = x / (x + y);

                    if(myg >= 1){
                        myg = myg - epsi;
                    }
                    if(myg <= 0){
                        myg = myg + epsi;
                    }

                    if (wasflipped == 1) {
                        wasflipped = 0;
                        myg = 1 - myg;
                    }
                    POSMAT(particle, edim) = myg;
                }

                /* Write new POSMAT into Ks to be passed into system */
                struct K pos;
                pos.k = VectorXd::Zero(Npars);
                for(int i = 0; i < Npars; i++){
                    pos.k(i) = POSMAT(particle, i);
                }
                //VectorXd XtPSO3 = VectorXd::Zero(nMoments);
                double cost = 0;
                X_t = (evolutionMatrix(pos.k, tf, nSpecies) * X_0.transpose()).transpose();
                XtmVec = moment_vector(X_t, nMoments);
                cost = calculate_cf2(YtmVec, XtmVec, weight);
                
                /* initialize PBMAT */
                for(int i = 0; i < Npars; i++){
                    PBMAT(particle, i) = POSMAT(particle, i);
                }
                PBMAT(particle, Npars) = cost; // add cost to final column
            }else{ 
                /* using new rate constants, initialize particle best values */
                /* step into PSO */
                double w1 = sfi * pUnifDist(pGenerator)/ sf2, w2 = sfc * pUnifDist(pGenerator) / sf2, w3 = sfs * pUnifDist(pGenerator)/ sf2;
                double sumw = w1 + w2 + w3; //w1 = inertial, w2 = pbest, w3 = gbest
                w1 = w1 / sumw; w2 = w2 / sumw; w3 = w3 / sumw;
                //w1 = 0.05; w2 = 0.90; w3 = 0.05;
                struct K pos;
                pos.k = VectorXd::Zero(Npars);
                pos.k = POSMAT.row(particle);
                VectorXd rpoint = comp_vel_vec(pos.k, particle, epsi, nan, hone);
                VectorXd PBVEC(Npars);
                for(int i = 0; i < Npars; i++){
                    PBVEC(i) = PBMAT(particle, i);
                }
                pos.k = w1 * rpoint + w2 * PBVEC + w3 * GBVEC; // update position of particle
                POSMAT.row(particle) = pos.k; // back into POSMAT
                
                double cost = 0;
                /* solve ODEs with new system and recompute cost */
                X_t = (evolutionMatrix(pos.k, tf, nSpecies) * X_0.transpose()).transpose();
                XtmVec = moment_vector(X_t, nMoments);
                cost = calculate_cf2(YtmVec, XtmVec, weight);
                
                /* update pBest and gBest */
                // #pragma omp critical
                // {
                if(cost < PBMAT(particle, Npars)){ // update particle best 
                    for(int i = 0; i < Npars; i++){
                        PBMAT(particle, i) = pos.k(i);
                    }
                    PBMAT(particle, Npars) = cost;
                    if(cost < gCost){ // update global 
                        gCost = cost;
                        GBVEC = pos.k;
                    }   
                }
                // }
            }
        }
        GBMAT.conservativeResize(GBMAT.rows() + 1, Npars + 1); // Add to GBMAT after each step.
        for (int i = 0; i < Npars; i++) {GBMAT(GBMAT.rows() - 1, i) = GBVEC(i);}
        GBMAT(GBMAT.rows() - 1, Npars) = gCost;

        sfi = sfi - (sfe - sfg) / nSteps2;   // reduce the inertial weight after each step 
        sfs = sfs + (sfe - sfg) / nSteps2;

    }
    cout << "GBMAT after targeted PSO:" << endl << GBMAT << endl;
    dist = calculate_cf1(tru.k, GBVEC);
    cout << "total difference b/w truk and final GBVEC:" << dist << endl; // compute difference
    
    ofstream plot;
	plot.open("GBMATP.csv");
	MatrixXd GBMATWithSteps(GBMAT.rows(), GBMAT.cols() + 1);
	VectorXd globalIterations(GBMAT.rows());
	for(int i = 0; i < GBMAT.rows(); i++){
		globalIterations(i) = i;
	}
	GBMATWithSteps << globalIterations, GBMAT;
	for(int i = 0; i < GBMATWithSteps.rows(); i++){
        for(int j = 0; j < GBMATWithSteps.cols(); j++){
            if(j == 0){
                plot << GBMATWithSteps(i,j);
            }else{
                plot << "," << GBMATWithSteps(i,j);
            }
        }
        plot << endl;
    }
	plot.close();

    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(t2 - t1).count();
    cout << "CODE FINISHED RUNNING IN " << duration << " s TIME!" << endl;

    return 0; // just to close the program at the end.
}

