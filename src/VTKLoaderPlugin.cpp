

#include "VTKLoaderPlugin.h"

#include "PointData.h"

#include "Set.h"
#include<iostream> 


#include<fstream> 
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

using namespace hdps;

// =============================================================================
// View
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
    const auto workingDirectory = getSetting("Data/WorkingDirectory", QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)).toString();
    //file_name.setFileMode(QFileDialog.ExistingFiles)

    QFileDialog fileDialog;
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    QStringList filePath = fileDialog.getOpenFileNames(nullptr, "Open a .vtk file", workingDirectory); // Open the file selector
    
    if (QFileInfo(filePath[0]).exists())
        setSetting("Data/WorkingDirectory", QFileInfo(filePath[0]).absoluteDir().absolutePath());

    std::string vtk = ".vtk"; 
    std::vector<int> incorrectIndex;

    for (int i = 0; i < filePath.length(); i++) {
        int found = filePath[i].toStdString().find(".vtk");
        if (found == -1) {
            incorrectIndex.push_back(i);
        }
    }

    if (incorrectIndex.size() != 0) {
            QMessageBox messageBox;
            messageBox.critical(0, "Error", "File(s) is/are not of type(s) .vtk"); // Throws error if file format is wrong
            messageBox.setFixedSize(500, 200);
        
    }
    else {
        std::string base_filename = filePath[0].toStdString().substr(filePath[0].toStdString().find_last_of("/\\") + 1);
        std::string::size_type const p(base_filename.find_last_of('.'));
        std::string fileName = base_filename.substr(0, p);
        auto points = _core->addDataset<Points>("Points", QString::fromStdString(fileName)); // create a datafile in the hdps core
        int timePoints = filePath.length();
        int numPoints;
        int xSize, ySize, zSize;
        int numDimensions = 8;
        // creates a 1D vector used to read the completed dataset into the hdps points datatype
        std::vector<float> dataSet;
        std::vector<std::vector<std::array<float,6 >>> flowLines;
        std::vector<std::vector<std::array<float,6 >>> flowLinesPrefix;
        int newTest = 0;
        std::string type;
        int flowLineSize = 0;
        std::string myString;
        std::string dimensions;
        int xCor, yCor, zCor;
        bool prefixVectorEngaged = false;
        bool prefixInitialized = false;
        int groupSize = filePath.size() / 30;
        int countLines = -1;
        int cumulativeLines = 0;

        // Notify the core system of the new data
        events().notifyDatasetAdded(points);
        QCoreApplication::processEvents();
        points->getDataHierarchyItem().setTaskRunning();
        points->getDataHierarchyItem().setTaskName("Load volume");
        points->getDataHierarchyItem().setTaskDescription("Allocating voxels");
        QCoreApplication::processEvents();

        QCoreApplication::processEvents();
        points->getDataHierarchyItem().setTaskDescription("Loading points");
        QCoreApplication::processEvents();

       
        for (int group = 0; group < groupSize; group++) {
           /* std::cout << countLines << std::endl;
            std::cout << cumulativeLines << std::endl;
            cumulativeLines = cumulativeLines + countLines+1;
            std::cout << cumulativeLines << std::endl;*/

            for (int t = 0; t < 30; t++) {
                
                

                int i = 0;
                int q = 0;
                if (t + 10 >= 30) {
                    q = t + 10 - 30 + group * 30;
                    //std::cout << "first 10" << std::endl;
                }
                else {
                    q = t + 10 + group * 30;
                    //std::cout << "last" << std::endl;

                }
                std::ifstream file(filePath[q].toStdString());
                std::cout << filePath[q].toStdString() << std::endl;
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
                getline(file, myString);
                seperatedLine = VTKLoaderPlugin::cutString(myString);
                type = seperatedLine[0];
                if (type == "SCALARS") {
                    std::cout << "wrong " << std::endl;
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
                        if ((z % 10) == 0) {
                            points->getDataHierarchyItem().setTaskProgress(z / static_cast<float>(zSize));
                            QCoreApplication::processEvents();
                        }
                    }
                    pointLocationsVector.~vector();
                }
                else if (type == "LINES") {
                    numDimensions = 3;
                    std::vector<std::vector<int>> lineIndexSize;

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
                    getline(file, myString);
                    getline(file, myString);
                    getline(file, myString);

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
                    std::vector<std::array<float, 6>> tempFlowLine;
                    
                    countLines = -1;
                    if (group == 0)
                    {
                        cumulativeLines = countLines;
                    }
                    else {
                        cumulativeLines = flowLines[0].size() - 1;
                    }
                    
                    
                    float previousIndex = -1;
                    int test = 0;

                    auto temp = lineIndex;
                    

                    
                    int l = 0;
                    for (int i = 0; i < pointLocationsVector.size(); i++) {
                        if (previousIndex != lineIndex[i])
                        {
                            l = 0;
                            countLines++;
                            cumulativeLines++;
                            previousIndex = lineIndex[i];
                            tempFlowLine.clear();
                            
                        }
                        tempFlowLine.push_back({ pointLocationsVector[i][0], pointLocationsVector[i][1], pointLocationsVector[i][2], speed[i], lineIndex[i], float(t) });
                        l++;

                        
                        
                        if (l == lineIndexSize[countLines][0])
                        {
                            if (t == 0) {
                                flowLines.push_back(tempFlowLine);
                                tempFlowLine.clear();
                                //std::cout << i / 8 << std::endl;
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
                if (((group * 30 + t) % 5) == 0) {
                    
                    points->getDataHierarchyItem().setTaskProgress((t+group*30) / static_cast<float>(groupSize * 30));
                    QCoreApplication::processEvents();
                }
            }
        }
        
        
        std::cout << "test1" << std::endl;
       
        for (int i = 0; i < flowLines.size(); i++) {
            for (int j = 0; j < flowLines[i].size(); j++) {
                /*if (flowLines[i][j][0] > 400) {
                    std::cout << "x: " << "i=" << i << "j= " << j  << "    " << flowLines[i][j][0] << "    " << flowLines[i][j][4] << std::endl;
                }
                if (flowLines[i][j][1] > 400) {
                    std::cout << "y: " << "i=" << i << "j= " << j  << "    " << flowLines[i][j][0] << "    " << flowLines[i][j][4] << std::endl;
                }*/
                //if (flowLines[i][j][2] > 400) {
                //    std::cout << "z: " << "i=" << i << "j= " << j << std::endl;
                //}
                //if (flowLines[i][j][4] == 0) {
                //    //std::cout << "found" << std::endl;
                //}
                dataSet.push_back(flowLines[i][j][0]);
                dataSet.push_back(flowLines[i][j][1]);
                dataSet.push_back(flowLines[i][j][2]);
                dataSet.push_back(flowLines[i][j][3]); 
                dataSet.push_back(flowLines[i][j][4]); 
                dataSet.push_back(flowLines[i][j][5]); 
            }
        }
        
        //points->getDataHierarchyItem().setTaskProgress(1.0f);
        points->setProperty("lineSize", flowLines[0].size());
        if (type == "LINES") {
            points->setData(dataSet.data(), flowLines.size()* (flowLines[0].size()), 6);
        }
        else {
            points->setData(dataSet.data(), numPoints* timePoints, numDimensions);
        }
        
        events().notifyDatasetChanged(points);
        
        points->getDataHierarchyItem().setTaskFinished();
        
        
    }
}

QIcon VTKLoaderPluginFactory::getIcon(const QColor& color /*= Qt::black*/) const
{
    return hdps::Application::getIconFont("FontAwesome").getIcon("cube", color);
}

VTKLoaderPlugin* VTKLoaderPluginFactory::produce()
{
    return new VTKLoaderPlugin(this);
}

hdps::DataTypes VTKLoaderPluginFactory::supportedDataTypes() const
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