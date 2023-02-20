#pragma once

#include <LoaderPlugin.h>

using namespace hdps::plugin;


// =============================================================================
// Loader
// =============================================================================

class VTKLoaderPlugin : public LoaderPlugin
{
    Q_OBJECT
public:
    VTKLoaderPlugin(const PluginFactory* factory) : LoaderPlugin(factory) { }
    ~VTKLoaderPlugin(void) override;
    
    void init() override;

    std::vector<std::string> cutString(std::string str);

    void loadData() Q_DECL_OVERRIDE;

private:
    unsigned int _numDimensions;

    QString _dataSetName;
};


// =============================================================================
// Factory
// =============================================================================

class VTKLoaderPluginFactory : public LoaderPluginFactory
{
    Q_INTERFACES(hdps::plugin::LoaderPluginFactory hdps::plugin::PluginFactory)
    Q_OBJECT
    Q_PLUGIN_METADATA(IID   "nl.tudelft.VTKLoaderPlugin"
                      FILE  "VTKLoaderPlugin.json")
    
public:
    VTKLoaderPluginFactory(void) {}
    ~VTKLoaderPluginFactory(void) override {}

    /**
     * Get plugin icon
     * @param color Icon color for flat (font) icons
     * @return Icon
     */
    QIcon getIcon(const QColor& color = Qt::black) const override;

    VTKLoaderPlugin* produce() override;

    hdps::DataTypes supportedDataTypes() const override;
};
