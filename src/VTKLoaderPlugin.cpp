

#include "VTKLoaderPlugin.h"

#include "PointData/PointData.h"
#include "Set.h"

#include <iostream>
#include <fstream>
#include <QtCore>
#include <QtDebug>
#include <QFileDialog>
#include <qmessagebox.h>

#include <random>
#include <vector>
#include <algorithm>
#include <string>



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
        std::vector<std::vector<std::array<float,4 >>> flowLines;
        
        std::string type;
        int flowLineSize = 0;
        std::string myString;
        std::string dimensions;
        int xCor, yCor, zCor;

        for (int t = 0; t < timePoints; t++) {
            // Notify the core system of the new data
            events().notifyDatasetAdded(points);
            QCoreApplication::processEvents();
            points->getDataHierarchyItem().setTaskRunning();
            points->getDataHierarchyItem().setTaskName("Load volume");
            points->getDataHierarchyItem().setTaskDescription("Allocating voxels");
            QCoreApplication::processEvents();

            
            int i = 0;
            //float b;
            std::ifstream file(filePath[t].toStdString());
            while (getline(file, myString)) {
                if (i == 4) {
                    dimensions = myString;
                }
                if (i == 5) {
                    std::cout << myString << std::endl;
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
            QCoreApplication::processEvents();
            points->getDataHierarchyItem().setTaskDescription("Loading points");

            std::array<float, 3> pointLocation = { 0,0,0 };
            std::vector<std::array<float, 3>> pointLocationsVector;
            QCoreApplication::processEvents();
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
                while (getline(file, myString)) {
                    if (myString.empty()) {
                        break;
                    }
                }
                getline(file, myString);
                getline(file, myString);
                getline(file, myString);
                std::vector<int> lineIndex;

                while (getline(file, myString)) {
                    if (myString.empty()) {
                        break;
                    }
                    else if (!isNumber(VTKLoaderPlugin::cutString(myString)[0])) {
                        break;
                    }
                    lineIndex.push_back(std::stoi(myString));
                }
                getline(file, myString);
                lineIndex.~vector();
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
                std::vector<std::array<float, 4>> tempFlowLine;
                for (int i = 0; i < pointLocationsVector.size(); i++) {
                    
                    tempFlowLine.push_back({ pointLocationsVector[i][0], pointLocationsVector[i][1], pointLocationsVector[i][2], speed[i]});
                    if ((i+1) % 8 == 0 && i!=0) {
                        if (t == 0) {
                            flowLines.push_back(tempFlowLine);
                            tempFlowLine.clear();
                        }
                        else {
                            flowLines[(i) / 8].push_back(tempFlowLine[3]);
                            flowLines[(i) / 8].push_back(tempFlowLine[4]);
                            flowLines[(i) / 8].push_back(tempFlowLine[5]);
                            flowLines[(i) / 8].push_back(tempFlowLine[6]);
                            flowLines[(i) / 8].push_back(tempFlowLine[7]);
                            tempFlowLine.clear();
                        }
                    }
                }
                tempFlowLine.~vector();
                
                flowLineSize = flowLines[0].size();
                std::cout << "size is = " << flowLines.size() << std::endl;
                std::cout << "size of line is = " << flowLineSize << std::endl;
            }
        }
        for (int i = 0; i < flowLines.size(); i++) {
            for (int j = 0; j < flowLineSize; j++) {
                dataSet.push_back(flowLines[i][j][0]);
                dataSet.push_back(flowLines[i][j][1]);
                dataSet.push_back(flowLines[i][j][2]);
                dataSet.push_back(flowLines[i][j][3]);
                
            }
        }
        points->getDataHierarchyItem().setTaskProgress(1.0f);
        points->setProperty("lineSize", flowLineSize);
    
        if (type == "LINES") {
            points->setData(dataSet.data(), flowLines.size()* flowLineSize, 4);
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