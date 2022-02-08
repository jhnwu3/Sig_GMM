#include "fileIO.hpp"

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;

/* Returns true if string is an integer*/
bool isNumber(const std::string& s)
{
    int it = 0;
    bool allDigits = true;
    while (it < s.length()){
        if (!(std::isdigit(s.at(it))) && !(s.at(it) == '\r') && !(s.at(it) == '\0') && !(s.at(it)==' ') && !(s.at(it)=='-')){ // check for control characters to check for numbers correctly.
            allDigits = false;
        }
        ++it; 
    } 
    return allDigits;
}

/* Returns true if string is a Double */
bool isDouble(const std::string& s)
{
    bool allDigits = true;
    int decimals = 0;
    int it = 0;
    for(int i = 0; i < s.length(); i++){
        if(s.at(i) == '.'){
            decimals++;
        }
    }
    if(decimals != 1){
        return false;
    }
     while (it < s.length()){
        if (!(std::isdigit(s.at(it))) && !(s.at(it) == '\r') && !(s.at(it) == '\0') && !(s.at(it) == '.')  && !(s.at(it)==' ') && !(s.at(it)=='-')){ // check for control characters to check for numbers correctly.
            allDigits = false;
        }
        ++it; 
    } 
    return allDigits; // -2, 1 for null, 1 for the .
}

/* Remove whitespace from string */
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

/* Finds next double in string position */
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

/* 
    Summary:
        Takes a txt file and converts it into a matrix 
    Input:
        fileName - name of file (with file extension) that's being converted
        rows - number of rows 
        cols - No. of Cols
    Output:
        A matrix of rows x cols defined in inputs.
    
*/
MatrixXd txtToMatrix(const string& fileName, int rows, int cols) {
    MatrixXd mat(rows, cols);
    std::ifstream in(fileName);
    // use first row to determine how many columns to read.
    for (int i = 0; i < rows; i++) {
        string line;
        if (in.is_open()) {
            std::getline(in, line);
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
    in.close();

    return mat;
}

/* 
    Summary:
        Same idea as above, but now can read an entire file and no longer need to specifiy number of columns.
    Input:
        path - file name with directory including the .csv extension
        fileSize - number of rows of matrix being outputted
    Output:
        Matrix of file with only the number of rows of fileSize.

*/
MatrixXd csvToMatrix (const std::string & path){
    std::ifstream indata;
    indata.open(path);
    if(!indata.is_open()){
        throw std::runtime_error("Invalid Sample File Name!");
        exit(EXIT_FAILURE);
    }
    std::string line;
    std::vector<double> values;
    unsigned int rows = 0;
    while (std::getline(indata, line)) {
        std::stringstream lineStream(line);
        std::string cell;
        while (std::getline(lineStream, cell, ',')) {
            values.push_back(std::stod(cell));
        }
        ++rows;
    }
    MatrixXd mat = MatrixXd::Zero(rows, values.size()/rows);
    int i = 0;
    for(int r = 0; r < rows; r++){
        for(int c = 0; c < mat.cols(); c++){
            mat(r,c) = values[i];
            i++;
        }
    }
 
    return mat;
}

/* 
    Summary:
        Reads a single file from the data/X directory into a matrix.
    Input:
        path - path of X directory
        xSize - number of rows of matrix to be returned
    Output:
        matrix of xSize rows.
*/
MatrixXd readX(const std::string &path){
    int nFile = 0;
    MatrixXd X_0;
    cout << "Reading data/X directory!" << endl;
    for(const auto & entry : fs::directory_iterator(path)){
        cout << entry << endl;
        X_0 = csvToMatrix(entry.path().string());
        ++nFile;
    }
    if(nFile < 1){
        cout << "Error! No X csv file detected in " << path << "!" << endl;
        exit(-1);
    }
    if(nFile > 1){
        cout << "Multiple X files detected, reading only the last X file read in." << endl;
    }
    return X_0;
}
/*
    Summary:
        Reads all files in data/Y directory into a list of matrices
    Input:
        path - path of Y directory
        ySize - number of rows of matrices to be returned
    Output:
        vector of matrices of ySize. note: vector == list.
*/
vector<MatrixXd> readY(const std::string & path){
    vector<MatrixXd> Y;
    cout << "Reading data/Y directory!" << endl;
    for(const auto & entry : fs::directory_iterator(path)){
        cout << entry << endl;
        Y.push_back(csvToMatrix(entry.path().string()));
    }
    if(Y.size() < 1){
        cout << "Error! 0 Y Files read in!" << endl;
        exit(-1);
    }
    return Y;
}
/* 
    Summary:
        Converts a matrix mat into a csv file, defined by the string variable fileName.
 */
void matrixToCsv(const MatrixXd& mat, const string& fileName){ // prints matrix to csv
    std::ofstream plot;
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

/*
    Read all Config.csv parameters into a set of parameters (as shown in the function parameters)

 */
int readCsvPSO(int &nPart1, int &nSteps1, int &nPart2, int &nSteps2, int &useOnlySecMom, int &useOnlyFirstMom, int &useLinear, int &nRuns, int &simulateYt, int &useInverse, int &nRates, int &thetaHeld, double &heldVal){
    std::ifstream input("../Config.csv");
    if(!input.is_open()){
        throw std::runtime_error("Could not open PSO file");
        return EXIT_FAILURE;
    }
    cout << "csvPSO" << endl;
    vector<double> params;
    string line;
    while(std::getline(input, line)){
        std::stringstream ss(line); // make a string stream from the line such that you can isolate each word even further.
        string col;
        while(std::getline(ss, col, ',')){
            if(isNumber(col) || isDouble(col)){ // only add into parameter vector if actually an int.
                params.push_back(std::stod(col)); 
            }
        }
    }
    cout << "params:" << params.size() << endl;
    nPart1 = params.at(0);
    nSteps1 = params.at(1);
    nPart2 = params.at(2);
    nSteps2 = params.at(3);
    useOnlySecMom = params.at(4);
    useOnlyFirstMom = params.at(5);
    useLinear = params.at(6);
    nRuns = params.at(7);
    simulateYt = params.at(8);
    useInverse = params.at(9);
    nRates = params.at(10);
    thetaHeld = params.at(11);
    heldVal = params.at(12);
    input.close();
    return 0;
}

// Reads Input Data Parameters.
int readCsvDataParam(int &nSpecies, int &nRates, int &xSize, int &ySize){
    std::ifstream input("../system_parameters.csv");
    cout << "csvData" << endl;
    if(!input.is_open()){
        throw std::runtime_error("Could not open data parameters file");
        return EXIT_FAILURE;
    }
    vector<int> params;
    string line;
    while(std::getline(input, line)){
        std::stringstream ss(line); // make a string stream from the line such that you can isolate each word even further.
        string col;
        while(std::getline(ss, col, ',')){
            if(isNumber(col)){ // only add into parameter vector if actually an int.
                params.push_back(std::stoi(col)); 
            }
        }
    }
    nSpecies = params.at(0);
    nRates = params.at(1);
    xSize = params.at(2);
    ySize = params.at(3);
    input.close();
    return 0;
}

// Reads Time Step Parameters.
VectorXd readCsvTimeParam(){

    std::ifstream input("../time_steps.csv");
    if(!input.is_open()){
        throw std::runtime_error("Could not open data parameters file");
        exit;
    }
    vector<double> params;
    string line;
    while(std::getline(input, line)){
        std::stringstream ss(line); // make a string stream from the line such that you can isolate each word even further.
        string col;
        while(std::getline(ss, col, ',')){
            if(isNumber(col) || isDouble(col)){ // only add into parameter vector if actually an int.
                params.push_back(std::stod(col)); 
            }
        }
    }
    VectorXd times(params.size());
    for(int i = 0; i < params.size(); i++){
        times(i) = params.at(i);
    }
    input.close();
    return times;
}

VectorXd readRates(int nRates){
    std::ifstream input("../true_rates.csv");
    if(!input.is_open()){
        throw std::runtime_error("Could not open data parameters file");
        exit;
    }
    vector<double> params;
    string line;
    while(std::getline(input, line)){
        std::stringstream ss(line); // make a string stream from the line such that you can isolate each word even further.
        string col;
        while(std::getline(ss, col, ',')){
            if(isNumber(col) || isDouble(col)){ // only add into parameter vector if actually an int.
                params.push_back(std::stod(col)); 
            }
        }
    }
    VectorXd rates(params.size());
    for(int i = 0; i < params.size(); i++){
        rates(i) = params.at(i);
    }
    input.close();
    if(rates.size() < nRates){
        cout << "Error, number of rates in parameters do not match number of true rates simulated!" << endl;
        exit(1);
    }
    return rates;
}