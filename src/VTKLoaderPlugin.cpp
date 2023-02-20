

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

    QString filePath = QFileDialog::getOpenFileName(nullptr, "Open a .vtk file", workingDirectory); // Open the file selector

    if (QFileInfo(filePath).exists())
        setSetting("Data/WorkingDirectory", QFileInfo(filePath).absoluteDir().absolutePath());

    std::string vtk = ".vtk"; 

    int found = filePath.toStdString().find(".vtk");
    

    if (found == -1) {
        QMessageBox messageBox;
        messageBox.critical(0, "Error", "File: " + filePath + " is not of type .vtk"); // Throws error if file format is wrong
        messageBox.setFixedSize(500, 200);
    }
    else {
        std::string base_filename = filePath.toStdString().substr(filePath.toStdString().find_last_of("/\\") + 1);
        std::string::size_type const p(base_filename.find_last_of('.'));
        std::string fileName = base_filename.substr(0, p);

        auto points = _core->addDataset<Points>("Points", QString::fromStdString(fileName)); // create a datafile in the hdps core

        // Notify the core system of the new data
        events().notifyDatasetAdded(points);

        QCoreApplication::processEvents();

        points->getDataHierarchyItem().setTaskRunning();
        points->getDataHierarchyItem().setTaskName("Load volume");

        points->getDataHierarchyItem().setTaskDescription("Allocating voxels");

        QCoreApplication::processEvents();

        int numDimensions = 7;
        

        std::string myString;
        std::string dimensions;
        int i = 0;
        //float b;
        std::ifstream file(filePath.toStdString());
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

        

        std::vector<std::string> seperatedLine = VTKLoaderPlugin::cutString(myString);
        int numPoints = std::stoi(seperatedLine[1]);

       seperatedLine = VTKLoaderPlugin::cutString(dimensions);
        int xSize = std::stoi(seperatedLine[1]);
        int ySize = std::stoi(seperatedLine[2]);
        int zSize = std::stoi(seperatedLine[3]);

        // creates a 1D vector used to read the completed dataset into the hdps points datatype
        std::vector<float> dataSet((xSize *ySize * zSize * numDimensions));

        int xCor, yCor, zCor;
       
        std::array<float, 3> pointLocation = {0,0,0};
        
        std::vector<std::array<float,3>> pointLocationsVector;


        QCoreApplication::processEvents();

        points->getDataHierarchyItem().setTaskDescription("Loading points");

        QCoreApplication::processEvents();
        
        while (getline(file, myString)) {
            
            if (VTKLoaderPlugin::cutString(myString)[0] == "POINT_DATA") {
                
                break;
            }

            seperatedLine = VTKLoaderPlugin::cutString(myString);
            pointLocation[0] = std::stof(seperatedLine[0]);
            pointLocation[1] = std::stof(seperatedLine[1]);
            pointLocation[2] = std::stof(seperatedLine[2]);
            
            pointLocationsVector.push_back(pointLocation);
        }

        getline(file, myString);
        getline(file, myString);

        std::vector<float> velocityMagnitude;
        while (getline(file, myString)) {
            if (VTKLoaderPlugin::cutString(myString)[0] == "VECTORS") {
                break;
            }
            velocityMagnitude.push_back(std::stof(myString));
        }

        std::array<float, 3> velocityVector = { 0,0,0 };

        std::vector<std::array<float, 3>> velocityVectorVector;

        while (getline(file, myString)) {

            if (VTKLoaderPlugin::cutString(myString)[0] == "") {

                break;
            }

            seperatedLine = VTKLoaderPlugin::cutString(myString);
            velocityVector[0] = std::stof(seperatedLine[0]);
            velocityVector[1] = std::stof(seperatedLine[1]);
            velocityVector[2] = std::stof(seperatedLine[2]);

            velocityVectorVector.push_back(velocityVector);
        }
        file.close();

        int iterator = 0;
        for (int z = 0; z < zSize; z++) {
            for (int y = 0; y < ySize; y++) {
                for (int x = 0; x < xSize; x++) {
                    for (int dim = 0; dim <7 ; dim++) {
                                
                        if (dim < 3) {
                            dataSet[iterator] = pointLocationsVector[iterator/7][dim];
                        }
                        else if (dim > 3) {
                            dataSet[iterator] = velocityMagnitude[iterator/7];
                        }
                        else {
                            dataSet[iterator] = velocityVectorVector[iterator/7][dim-4];
                        }
                    }
                        iterator++;
                       

                        
                        
                }
            }
            if ((z % 10) == 0) {
                points->getDataHierarchyItem().setTaskProgress(z / static_cast<float>(zSize));
                QCoreApplication::processEvents();
            }
        }

         
        points->getDataHierarchyItem().setTaskProgress(1.0f);

        points->setData(dataSet.data(), numPoints, 7);
        
        events().notifyDatasetChanged(points);
        
        points->getDataHierarchyItem().setTaskFinished();

        //file.read((char*)&a, sizeof(a)); // read the number of points as stated in the vtk metadata (this excludes missing data)
        //auto numPoints = a;
        //std::cout << a << std::endl;
        //file.read((char*)&a, sizeof(a)); // read the number of dimensions stated in the vtk metadata
        //int numDimensions = a;

    //    std::vector<int16_t> dimensions(6); // creates vector for holding vtk size data
    //    

    //    for (int i = 0; i < 6;i++) {
    //        file.read((char*)&a, sizeof(a));
    //        dimensions[i] = a; // reads the minimum and maximum xyz values from vtk metadata in the following order. xMin,xMax,yMin,yMax,zMin,zMax
    //    }

    //    std::vector<int16_t> coordinates(3);// empty vector to hold coordinate data for current point in loop to be placed in dataVector

    //    int xSize = dimensions[1] - dimensions[0];// calculate xSize
    //    int ySize = dimensions[3] - dimensions[2];// calculate ySize
    //    int zSize = dimensions[5] - dimensions[4];// calculate zSize

    //    // creates a 4D dataVector of size (xSize,ySize,zSize,numDimensions) with inial values 0
    //    std::vector<std::vector<std::vector<std::vector<float>>>> dataArray(xSize, std::vector<std::vector<std::vector<float>>>(ySize, std::vector<std::vector<float>>(zSize,std::vector<float>(numDimensions,0))));
    //    // creates a 4D binary objectMask vector used for resetting non-Object points (so missing data) back to 0 after scaling has been performed
    //    std::vector<std::vector<std::vector<std::vector<float>>>> backgroundMask(xSize, std::vector<std::vector<std::vector<float>>>(ySize, std::vector<std::vector<float>>(zSize, std::vector<float>(numDimensions, 1))));
    //    
    //    // creates a 1D vector used to read the completed dataset into the hdps points datatype
    //    std::vector<float> dataSet((xSize * ySize*zSize* numDimensions));
    //    
    //    // creates 1D vectors of size numDimensions that are used to contain the minimum and maximum values in each dimension for scaling purposes
    //    std::vector<float> dimensionMinimum(numDimensions, 0);
    //    std::vector<float> dimensionMaximum(numDimensions, 0);

    //    int zCor;
    //    int yCor;
    //    int xCor;
    //    int index=0;
    //    int dataIterator;
    //    
    //    QCoreApplication::processEvents();

    //    points->getDataHierarchyItem().setTaskDescription("Loading points");

    //    QCoreApplication::processEvents();

    //    for (int k = 0; k < numPoints; k++) { // loop through all the datapoint specified in the vtk file
    //        for (int t = 0; t < 3; t++) { // loop through xyz values of current datapoint
    //            file.read((char*)&a, 4);
    //            coordinates[t] = a; // Read coordinates off current datapoint
    //            
    //        }
    //        xCor = coordinates[0] - 1;
    //        yCor = coordinates[1] - 1;
    //        zCor = coordinates[2] - 1;
    //        for (int i = 0; i < numDimensions; i++) // loop through all dimensions of current datapoint
    //        {   
    //            file.read((char*)&b, 4);

    //            dataArray[xCor][yCor][zCor][i] = b; // read current dimension of current datapoint into the dataVector
    //            backgroundMask[xCor][yCor][zCor][i] = 0; // set the current datapoint to 0 for non-missing data
    //            
    //            if (k == 0) {// if its the firs itteration set initial minimum
    //                dimensionMinimum[i] = b; 
    //            }
    //            if (b < dimensionMinimum[i]) { // otherwise only change the minimum if the current value is lower than the lowest value currently stored
    //                dimensionMinimum[i] = b;
    //            }
    //        }

    //        if ((k % 1000) == 0) {
    //            points->getDataHierarchyItem().setTaskProgress(k / static_cast<float>(numPoints));
    //            QCoreApplication::processEvents();
    //        }
    //            
    //    }

    //    points->getDataHierarchyItem().setTaskProgress(1.0f);

    //    QCoreApplication::processEvents();

    //    file.close(); // stop reading the file

    //    
    //    points->getDataHierarchyItem().setTaskDescription("Masking points");
    //    points->getDataHierarchyItem().setTaskProgress(0.0f);


    //    std::vector<unsigned int, std::allocator<unsigned int>> selectionIndices;

    //    selectionIndices.reserve(xSize * ySize * zSize);

    //    int iterator = 0;
    //    int selectCount = 0;
    //    
    //    //loop through the dataArray to scale all the datapoints and set background to minimum object value -1
    //    for (int z = 0; z < zSize; z++) {
    //        for (int y = 0; y < ySize; y++) {
    //            for (int x = 0; x < xSize; x++) {
    //                for (int dim = 0; dim < numDimensions; dim++) {
    //                    
    //                    

    //                    dataSet[iterator] = dataArray[x][y][z][dim]+backgroundMask[x][y][z][dim]*(dimensionMinimum[dim]-1);

    //                    // record indices containing only Object coordinates
    //                    if (dataSet[iterator] != (dimensionMinimum[dim] - 1) && iterator % numDimensions == 0) {
    //                        selectionIndices.push_back(iterator/numDimensions);
    //                        selectCount++;
    //                    }
    //                    iterator++;
    //                }
    //            }
    //        }

    //        if ((z % 10) == 0) {
    //            points->getDataHierarchyItem().setTaskProgress(z / static_cast<float>(zSize));
    //            QCoreApplication::processEvents();
    //        }
    //        
    //    }
    //
    //    selectionIndices.resize(selectCount);

    //    
    //    points->getDataHierarchyItem().setTaskProgress(1.0f);
    //    
    //    points->setData(dataSet.data(), xSize*ySize*zSize, numDimensions);// pass 1D vector to points data in core
    //    
    //    //declare the x,y and z sizes of the data which is needed to create the imagedata object in the 3D viewer
    //    points->setProperty("xDim", xSize);
    //    points->setProperty("yDim", ySize);
    //    points->setProperty("zDim", zSize);
    //    std::vector<QString> names = { "xDim", "yDim", "zDim" };
    //    points->setDimensionNames(names);
    //    events().notifyDatasetChanged(points);

    //    points->getDataHierarchyItem().setTaskDescription("Creating subset");


    //    // create automatic object only subset
    //    points->setSelectionIndices(selectionIndices);
    //    auto subset = points->createSubsetFromSelection("Object Only", points);

    //    // Notify the core system of the new data
    //    events().notifyDatasetAdded(subset);

    //    points->selectNone();

    //    points->getDataHierarchyItem().setTaskFinished();
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