

#include "VTKLoaderPlugin.h"

#include "PointData/PointData.h"
#include "Set.h"


#include <iostream>
#include <fstream>
#include <ClusterData/ClusterData.h>


#include <QtCore>
#include <QtDebug>
#include <QFileDialog>
#include <qmessagebox.h>

#include <random>
#include <vector>
#include <deque>
#include <algorithm>
#include <string>

#include<chrono> 
#include<thread>



Q_PLUGIN_METADATA(IID "nl.tudelft.VTKLoaderPlugin")

using namespace mv;

// =============================================================================
// VTK loader plugin, created in order to fascilitate the loading in of 4D flow pathline data.
// Written by: Mitchell Martijn de Boer
// =============================================================================

VTKLoaderPlugin::~VTKLoaderPlugin(void)
{

}

/**
 * Mandatory plugin override function. Any initial state can be set here.
 * This function gets called when an instance of the plugin is created.
 * In this case when someone select the loader option from the menu.
 */
void VTKLoaderPlugin::init()
{

}

/**
 * Funtion the loads in the data and transforms it from its file type to pointsdata.
 * It also fills in missing data in the input volume with values 1 smaller than the smallest value in the dimension
 * (needed for visualization).
 */
void VTKLoaderPlugin::loadData()
{
    // Set current working directory.
    const auto workingDirectory = getSetting("Data/WorkingDirectory", QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)).toString();
    
    // Read selected files from file selector.
    QFileDialog fileDialog;
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    QStringList filePath = fileDialog.getOpenFileNames(nullptr, "Open a .vtk file", workingDirectory); // Open the file selector


    if (QFileInfo(filePath[0]).exists())
        setSetting("Data/WorkingDirectory", QFileInfo(filePath[0]).absoluteDir().absolutePath());

    std::vector<int> incorrectIndex;

    // Check if filetype == .vtk
    for (int i = 0; i < filePath.length(); i++) {
        int found = filePath[i].toStdString().find(".vtk");
        if (found == -1) {
            incorrectIndex.push_back(i);
        }
    }

    // If the type is wrong, throw error, else start data loading.
    if (incorrectIndex.size() != 0) {
        QMessageBox messageBox;
        messageBox.critical(0, "Error", "File(s) is/are not of type(s) .vtk"); // Throws error if file format is wrong
        messageBox.setFixedSize(500, 200);

    }
    else {
        // Convert string of file names, to individual correct file names.
        std::string base_filename = filePath[0].toStdString().substr(0,filePath[0].toStdString().find_last_of("/\\"));
        std::string fileName = base_filename.substr(base_filename.find_last_of("/\\") + 1);
        
        // Create empty pointdata to load the dataset into.
        auto points = mv::data().createDataset<Points>("Points", QString::fromStdString(fileName));
        int timePoints = filePath.length();
        int numPoints;
        int xSize, ySize, zSize;
        int numDimensions = 8;

        // Creates a 1D vector used to read the completed dataset into the mv points datatype.
        std::vector<float> dataSet;
        std::vector<std::vector<std::array<float, 7 >>> flowLines;
        std::vector<std::vector<std::array<float, 7 >>> flowLinesPrefix;
        std::vector<float> pathlines;
        int newTest = 0;
        std::string type;
        int flowLineSize = 0;
        std::string myString;
        std::string dimensions;
        int xCor, yCor, zCor;
        bool prefixVectorEngaged = false;

        // Calculates the number of seperate datagroups, this is done specifically for the 4D flow dataset that was used in the thesis 
        // "Stochastic Neighbor Embedding for interactive visualization of flow patterns in 4D flow MRI" by Mitchell de Boer. This is
        // because of the fact that this dataset consisted of path lines subdivided into flow components with each having 30 timepoints.
        // Meaning that this calculates the amount of flow components added based on the amount of imported files.
        int groupSize = filePath.size() / 30;
        
        
        int countLines = -1;
        int cumulativeLines = 0;
        int cumulativeLinesSaved = 0;

        // Notify the core system of the new data
        mv::events().notifyDatasetAdded(points);
        
        std::vector<std::array<float, 3>> previousLocationVector;
        int resetPoint;

        // Loop over all timepoints.
        for (int t = 0; t < 30; t++) {
            int i = 0;   
            // Open the current file.
            std::ifstream file(filePath[t].toStdString());       

            // Record metadata. (might be no longer necessary because this is done later as well)
            while (getline(file, myString)) {
                if (i == 4) {
                    dimensions = myString;
                }
                if (i == 5) {
                    break;
                }
                i++;
            }
            if (!dimensions.empty()) {
                std::vector<std::string> seperatedLine = VTKLoaderPlugin::cutString(myString);
                numPoints = std::stoi(seperatedLine[1]);
                seperatedLine = VTKLoaderPlugin::cutString(dimensions);
                xSize = std::stoi(seperatedLine[1]);
                ySize = std::stoi(seperatedLine[2]);
                zSize = std::stoi(seperatedLine[3]);
            }
            // Initializes vectors for data recording.
            std::array<float, 3> pointLocation = { 0,0,0 };
            std::vector<std::array<float, 3>> pointLocationsVectorTemp;

            // Reads in the location of points. (might be no longer necessary because this is done later as well)
            std::vector<std::string> seperatedLine;
            while (getline(file, myString)) {
                if (myString.empty()) {
                    break;
                }
                else if (!isNumber(VTKLoaderPlugin::cutString(myString)[0])) {
                    break;
                }
                else {
                    seperatedLine = VTKLoaderPlugin::cutString(myString);
                    pointLocation[0] = std::stof(seperatedLine[0]);
                    pointLocation[1] = std::stof(seperatedLine[1]);
                    pointLocation[2] = std::stof(seperatedLine[2]);
                    pointLocationsVectorTemp.push_back(pointLocation);
                }
            }
            file.close();

            
            
            // Because files were not fully ordered from start to finish, this loop records the point at which vectors no longer line up with the previous file. 
            // The files after are then put in front of this first group of time points.
            if (t != 0) {
                if (pointLocationsVectorTemp[0][0] != previousLocationVector[5][0] && pointLocationsVectorTemp[0][1] != previousLocationVector[5][1] && pointLocationsVectorTemp[0][2] != previousLocationVector[5][2])
                {
                    std::cout << "reset found at t =" << t << std::endl;
                    resetPoint = t;
                }
            }
            previousLocationVector = pointLocationsVectorTemp;
        }
    


        // Loops over all flow components.
        for (int group = 0; group < groupSize; group++) {
           
            // Loop over all timepoints
            for (int t = 0; t < 30; t++) {


                // Initialize indexing parameters
                int i = 0;
                int q = 0;

                //Adjust the timpoint counter in order to load in data in the propper order
                if (t + resetPoint >= 30) {
                    q = t + resetPoint - 30 + group * 30;
                }
                else {
                    q = t + resetPoint + group * 30;
                }

                // Open current timepoint file.
                std::ifstream file(filePath[q].toStdString());
                
                // Record metadata.
                while (getline(file, myString)) {
                    if (i == 4) {
                        dimensions = myString;
                    }
                    if (i == 5) {
                        break;
                    }
                    i++;
                }
                if (!dimensions.empty()) {
                    std::vector<std::string> seperatedLine = VTKLoaderPlugin::cutString(myString);
                    numPoints = std::stoi(seperatedLine[1]);
                    seperatedLine = VTKLoaderPlugin::cutString(dimensions);
                    xSize = std::stoi(seperatedLine[1]);
                    ySize = std::stoi(seperatedLine[2]);
                    zSize = std::stoi(seperatedLine[3]);
                }

                std::array<float, 3> pointLocation = { 0,0,0 };
                std::vector<std::array<float, 3>> pointLocationsVector;

                // Read point locataions. (mainly done in order to get to the end of the file)
                std::vector<std::string> seperatedLine;
                while (getline(file, myString)) {
                    if (myString.empty()) {
                        break;
                    }
                    else if (!isNumber(VTKLoaderPlugin::cutString(myString)[0])) {
                        break;
                    }
                    else {
                        seperatedLine = VTKLoaderPlugin::cutString(myString);
                        pointLocation[0] = std::stof(seperatedLine[0]);
                        pointLocation[1] = std::stof(seperatedLine[1]);
                        pointLocation[2] = std::stof(seperatedLine[2]);
                        pointLocationsVector.push_back(pointLocation);
                    }
                }
                 
                numPoints = pointLocationsVector.size();           

                // Record what type of data is loaded in.
                getline(file, myString);
                seperatedLine = VTKLoaderPlugin::cutString(myString);
                type = seperatedLine[0];

                // "scalars" is old depricated code for when i was working with vectorfields instead of pathlines. 
                // This used to be a function that checked whether scalar points were loaded or pathlines.
                if (type == "SCALARS") {
                    getline(file, myString);
                    std::vector<float> velocityMagnitude;
                    while (getline(file, myString)) {
                        if (!isNumber(VTKLoaderPlugin::cutString(myString)[0])) {
                            break;
                        }
                        velocityMagnitude.push_back(std::stof(myString));
                    }
                    std::array<float, 3> velocityVector = { 0,0,0 };
                    std::vector<std::array<float, 3>> velocityVectorVector;

                    while (getline(file, myString)) {

                        if (!isNumber(VTKLoaderPlugin::cutString(myString)[0])) {

                            break;
                        }

                        seperatedLine = VTKLoaderPlugin::cutString(myString);
                        velocityVector[0] = std::stof(seperatedLine[0]);
                        velocityVector[1] = std::stof(seperatedLine[1]);
                        velocityVector[2] = std::stof(seperatedLine[2]);
                        velocityVectorVector.push_back(velocityVector);
                    }
                    velocityVector.~array();
                    file.close();

                    int iterator = 0;
                    for (int z = 0; z < zSize; z++) {
                        for (int y = 0; y < ySize; y++) {
                            for (int x = 0; x < xSize; x++) {
                                for (int dim = 0; dim < numDimensions; dim++) {
                                    if (dim < 3) {
                                        dataSet.push_back(pointLocationsVector[iterator / numDimensions][dim]);
                                    }
                                    else if (dim == 3) {
                                        dataSet.push_back(velocityMagnitude[iterator / numDimensions]);
                                    }
                                    else if (dim == 7) {
                                        dataSet.push_back(t);
                                    }
                                    else {
                                        dataSet.push_back(velocityVectorVector[iterator / numDimensions][dim - 4]);
                                    }
                                    iterator++;
                                }
                            }
                        }
                    }
                    pointLocationsVector.~vector();
                }
                else if (type == "LINES") {
                    numDimensions = 3;
                    std::vector<std::vector<int>> lineIndexSize;

                    // Records the size of pathlines, in order to make them all have the same amount of datapoints at the end.
                    std::vector<int> tempLineIndex;
                    while (getline(file, myString)) {
                        if (myString.empty()) {
                            break;
                        }
                        auto seperatedVector = VTKLoaderPlugin::cutString(myString);
                        for (int k = 0; k < seperatedVector.size(); k++)
                        {
                            tempLineIndex.push_back(std::stoi(seperatedVector[k]));
                        }
                        lineIndexSize.push_back(tempLineIndex);
                        tempLineIndex.clear();
                    }
                    // Skip irrelevent metadata.
                    getline(file, myString);
                    getline(file, myString);
                    getline(file, myString);

                    // Records the indices of pathlines, is later used in order to allign the same pathline for over multiple timepoints.
                    std::vector<float> lineIndex;
                    while (getline(file, myString)) {
                        if (myString.empty()) {
                            break;
                        }
                        else if (!isNumber(VTKLoaderPlugin::cutString(myString)[0])) {
                            break;
                        }
                        lineIndex.push_back(std::stof(myString));
                    }
                    getline(file, myString);

                    // Record the velocity magnitude for points along pathlines.
                    std::vector<float> speed;
                    while (getline(file, myString)) {
                        if (myString.empty()) {
                            break;
                        }
                        else if (!isNumber(VTKLoaderPlugin::cutString(myString)[0])) {
                            break;
                        }
                        speed.push_back(std::stof(myString));
                    }


                    getline(file, myString);
                    std::vector<int> ID;
                    while (getline(file, myString)) {
                        if (myString.empty()) {
                            break;
                        }
                        else if (!isNumber(VTKLoaderPlugin::cutString(myString)[0])) {
                            break;
                        }
                        ID.push_back(std::stoi(myString));
                    }
                    file.close();
                    ID.~vector();
                    int iterator = 0;
                    std::vector<std::array<float, 7>> tempFlowLine;


                    // Count the number of pathlines read in up to this points.
                    countLines = -1;
                    if (group == 0)
                    {
                        cumulativeLines = countLines;
                    }
                    else if (t == 0) {
                        cumulativeLines = flowLines.size() - 1;
                        cumulativeLinesSaved = cumulativeLines;
                    }
                    else {
                        cumulativeLines = cumulativeLinesSaved;
                    }

                    float previousIndex = -1;
                    int test = 0;
                    auto temp = lineIndex;

                    int l = 0;

                    // Loop actually reading in pathline data.
                    for (int i = 0; i < pointLocationsVector.size(); i++) {
                        // Checks if we have landed on a new pathine segment.
                        if (test == 0)
                        {
                            l = 0;
                            test = 1;
                            countLines++;
                            cumulativeLines++;
                            previousIndex = lineIndex[i];
                            
                            tempFlowLine.clear();

                        }
                        // Record current points in pathline.
                        tempFlowLine.push_back({ pointLocationsVector[i][0], pointLocationsVector[i][1], pointLocationsVector[i][2], speed[i], lineIndex[i], float(t), float(group) });                        
                        l++;

                        // Checks if the end of this line segment has been reached, if so record the temporary vector into a permanent one containing already recorded pathline segments.
                        if (l == lineIndexSize[countLines][0])
                        {
                            test = 0;                            
                            if (t == 0) {
                                flowLines.push_back(tempFlowLine);
                                tempFlowLine.clear();                               
                            }
                            else {
                                int copy = 0;
                                for (int j = 3; j < l; j++)
                                {
                                    flowLines[cumulativeLines].push_back(tempFlowLine[j]);
                                    copy = j;
                                }
                                if (l != 8) {
                                    for (int j = 0; j < 8 - l; j++)
                                    {
                                        flowLines[cumulativeLines].push_back(tempFlowLine[copy]);
                                    }
                                }
                            }
                        }
                    }
                    
                }

            }
        }
        // After all data has been loaded in, convert it into the created points dataset, I think the pathlines vector could be removed, but im not 100% about that.
        int q = 0;
        for (int i = 0; i < flowLines.size(); i++) {
            for (int j = 0; j < flowLines[i].size(); j++) {

                pathlines.push_back(flowLines[i][j][0]);
                pathlines.push_back(flowLines[i][j][1]);
                pathlines.push_back(flowLines[i][j][2]);
                pathlines.push_back(flowLines[i][j][3]);
                pathlines.push_back(flowLines[i][j][5]);

                dataSet.push_back(flowLines[i][j][0]);
                dataSet.push_back(flowLines[i][j][1]);
                dataSet.push_back(flowLines[i][j][2]);

                dataSet.push_back(flowLines[i][j][3]);
                dataSet.push_back(flowLines[i][j][4]);
                dataSet.push_back(flowLines[i][j][5]);
                dataSet.push_back(flowLines[i][j][6]);

                // Calculate the vectors at timepoints.
                if (j == flowLines[i].size() - 1) {
                    dataSet.push_back(flowLines[i][j][0] - flowLines[i][j - 1][0]);
                    dataSet.push_back(flowLines[i][j][1] - flowLines[i][j - 1][1]);
                    dataSet.push_back(flowLines[i][j][2] - flowLines[i][j - 1][2]);
                }
                else {
                    dataSet.push_back(flowLines[i][j + 1][0] - flowLines[i][j][0]);
                    dataSet.push_back(flowLines[i][j + 1][1] - flowLines[i][j][1]);
                    dataSet.push_back(flowLines[i][j + 1][2] - flowLines[i][j][2]);
                }
                
                
            }
        }

        
        points->setProperty("lineSize", flowLines[0].size());

        // Put dataset into the points object.
        if (type == "LINES") {
            points->setData(dataSet.data(), flowLines.size(), flowLines[0].size() * 10);
        }
        else {
            points->setData(dataSet.data(), numPoints * timePoints, numDimensions);
        }

        // Add dimension names.
        std::vector<QString> dimNames;
        int alterator = 0;
        for (int i = 0; i < flowLines[0].size() * 10; i++)
        {
            if (alterator == 10) {
                alterator = 0;
            }
            if (alterator == 0) {
                QString string = QString("x") + QString::number(i / 10);
                dimNames.push_back(string);
            }
            else if (alterator == 1) {
                QString string = QString("y") + QString::number(i / 10);
                dimNames.push_back(string);
            }
            else if (alterator == 2) {
                QString string = QString("z") + QString::number(i / 10);
                dimNames.push_back(string);
            }
            else if (alterator == 3) {
                QString string = QString("speed") + QString::number(i / 10);
                dimNames.push_back(string);
            }
            else if (alterator == 4) {
                QString string = QString("index") + QString::number(i / 10);
                dimNames.push_back(string);
            }
            else if (alterator == 5) {
                QString string = QString("time") + QString::number(i / 10);
                dimNames.push_back(string);
            }
            else if (alterator == 6) {
                QString string = QString("group") + QString::number(i / 10);
                dimNames.push_back(string);
            }else if (alterator == 7) {
                QString string = QString("x'") + QString::number(i / 10);
                dimNames.push_back(string);
            }
            else if (alterator == 8) {
                QString string = QString("y'") + QString::number(i / 10);
                dimNames.push_back(string);
            }
            else {
                QString string = QString("z'") + QString::number(i / 10);
                dimNames.push_back(string);
            }


            alterator++;

        }

       
        points->setDimensionNames(dimNames);
        mv::events().notifyDatasetDataChanged(points);



    }
}

QIcon VTKLoaderPluginFactory::getIcon(const QColor& color /*= Qt::black*/) const
{
    return mv::Application::getIconFont("FontAwesome").getIcon("cube", color);
}

VTKLoaderPlugin* VTKLoaderPluginFactory::produce()
{
    return new VTKLoaderPlugin(this);
}

mv::DataTypes VTKLoaderPluginFactory::supportedDataTypes() const
{
    DataTypes supportedTypes;
    supportedTypes.append(PointType);
    return supportedTypes;
}

std::vector<std::string> VTKLoaderPlugin::cutString(std::string str)
{

    std::vector<std::string> spaceSeperated;
    std::string word = "";
    for (auto x : str)
    {
        if (x == ' ')
        {
            spaceSeperated.push_back(word);
            word = "";

        }
        else {
            word = word + x;
        }
    }
    spaceSeperated.push_back(word);

    return spaceSeperated;
}

bool VTKLoaderPlugin::isNumber(std::string str)
{
    /*for (char const& c : str) {
        if (std::isdigit(c) == 0) return false;
    }
    return true;*/
    return str.find_first_not_of("0123456789.-") == std::string::npos;
}