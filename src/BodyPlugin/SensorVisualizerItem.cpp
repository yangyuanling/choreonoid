/*!
  @file
  @author Shin'ichiro Nakaoka
*/

#include "SensorVisualizerItem.h"
#include "BodyItem.h"
#include <cnoid/BasicSensors>
#include <cnoid/RangeCamera>
#include <cnoid/RangeSensor>
#include <cnoid/PointSetItem>
#include <cnoid/ItemTreeView>
#include <cnoid/ItemManager>
#include <cnoid/SceneGraph>
#include <cnoid/SceneDrawables>
#include <cnoid/MeshGenerator>
#include <cnoid/SceneProvider>
#include <cnoid/ImageProvider>
#include <cnoid/ConnectionSet>
#include <cnoid/Archive>
#include "gettext.h"

using namespace std;
using namespace cnoid;

namespace {

class Arrow : public SgPosTransform
{
public:
    SgUpdate update;
    SgPosTransformPtr cylinderPosition;
    SgScaleTransformPtr cylinderScale;
    SgShapePtr cylinder;
    SgPosTransformPtr conePosition;
    SgShapePtr cone;
    
    Arrow(SgShape* cylinder, SgShape* cone)
        : cylinder(cylinder),
          cone(cone)
    {
        cylinderScale = new SgScaleTransform();
        cylinderScale->addChild(cylinder);
        cylinderPosition = new SgPosTransform();
        cylinderPosition->addChild(cylinderScale);
        addChild(cylinderPosition);

        conePosition = new SgPosTransform();
        conePosition->addChild(cone);
        addChild(conePosition);

        setVector(Vector3(0.0, 0.0, 0.0));
    }

    void setVector(const Vector3& v) {
        double len = v.norm();
        cylinderScale->setScale(Vector3(1.0, len, 1.0));
        cylinderPosition->setTranslation(Vector3(0.0, len / 2.0, 0.0));
        conePosition->setTranslation(Vector3(0.0, len, 0.0));

        if(len > 0.0){
            Vector3 axis = (Vector3::UnitY().cross(v)).normalized();
            double angle = acos(Vector3::UnitY().dot(v) / len);
            setRotation(AngleAxis(angle, axis));
        }
            
        notifyUpdate(update);
    }
};

typedef ref_ptr<Arrow> ArrowPtr;

class SensorVisualizerBase
{
public:
    /*
    virtual void onSensorPositionChanged() = 0;
    virtual void onSensorStateChanged() = 0;
    */
};


class ForceSensorVisualizerItem : public Item, public SensorVisualizerBase, public SceneProvider
{
public :
    ForceSensorVisualizerItem();
    virtual SgNode* getScene();
    void setBodyItem(BodyItem* bodyItem);
    void setVisualRatio(double visualRatio);
    void onSensorPositionsChanged();
    void updateSensorState();
    void updateForceSensorState(int index);
    void onForceSensorStateChanged(int index);

    virtual void doPutProperties(PutPropertyFunction& putProperty);
    virtual bool store(Archive& archive);
    virtual bool restore(const Archive& archive);

    SgGroupPtr scene;
    SgShapePtr cylinder;
    SgShapePtr cone;
    DeviceList<ForceSensor> forceSensors;
    vector<ArrowPtr> forceSensorArrows;
    double visualRatio;
    ScopedConnectionSet connections;
};

class PointCloudVisualizerItem : public PointSetItem, public SensorVisualizerBase
{
public :
    PointCloudVisualizerItem();
    void setBodyItem(BodyItem* bodyItem, RangeCamera* rangeCamera);
    void onSensorPositionsChanged();
    void updateRangeCameraState();

    RangeCamera* rangeCamera;
    ScopedConnectionSet connections;
};

class RangeSensorVisualizerItem : public PointSetItem, public SensorVisualizerBase
{
public :
    RangeSensorVisualizerItem();
    void setBodyItem(BodyItem* bodyItem, RangeSensor* rangeSensor);
    void onSensorPositionsChanged();
    void updateRangeSensorState();

    RangeSensor* rangeSensor;
    ScopedConnectionSet connections;
};

class CameraImageVisualizerItem : public Item, public ImageProvider, public SensorVisualizerBase
{
public :
    CameraImageVisualizerItem();
    virtual const Image* getImage();
    void setBodyItem(BodyItem* bodyItem, Camera* camera);
    void updateCameraImage();

    Camera* camera;
    ScopedConnectionSet connections;
    std::shared_ptr<const Image> image;
};

}

namespace cnoid {

class SensorVisualizerItemImpl
{
public:
    SensorVisualizerItem* self;
    BodyItem* bodyItem;
    vector<Item*> subItems;
    vector<ItemPtr> restoredSubItems;

    SensorVisualizerItemImpl(SensorVisualizerItem* self);
    void onPositionChanged();
};

}


void SensorVisualizerItem::initializeClass(ExtensionManager* ext)
{
    ItemManager& im = ext->itemManager();
    im.registerClass<SensorVisualizerItem>(N_("SensorVisualizer"));
    im.addCreationPanel<SensorVisualizerItem>();

    im.registerClass<ForceSensorVisualizerItem>(N_("ForceSensorVisualizer"));
    im.registerClass<PointCloudVisualizerItem>(N_("PointCloudVisualizer"));
    im.registerClass<RangeSensorVisualizerItem>(N_("RangeSensorVisualizer"));
    im.registerClass<CameraImageVisualizerItem>(N_("CameraImageVisualizer"));
}


SensorVisualizerItem::SensorVisualizerItem()
{
    impl = new SensorVisualizerItemImpl(this);
}


SensorVisualizerItemImpl::SensorVisualizerItemImpl(SensorVisualizerItem* self)
    : self(self)
{

}


SensorVisualizerItem::SensorVisualizerItem(const SensorVisualizerItem& org)
    : Item(org)
{
    impl = new SensorVisualizerItemImpl(this);
}


SensorVisualizerItem::~SensorVisualizerItem()
{
    delete impl;
}


Item* SensorVisualizerItem::doDuplicate() const
{
    return new SensorVisualizerItem(*this);
}


void SensorVisualizerItem::onPositionChanged()
{
    if(parentItem()){
        impl->onPositionChanged();
    }
}


void SensorVisualizerItemImpl::onPositionChanged()
{
    BodyItem* newBodyItem = self->findOwnerItem<BodyItem>();
    if(newBodyItem != bodyItem){
        bodyItem = newBodyItem;
        for(size_t i=0; i < subItems.size(); i++){
            subItems[i]->detachFromParentItem();
        }
        subItems.clear();

        int n = restoredSubItems.size();
        int j = 0;

        if(bodyItem){
            Body* body = bodyItem->body();

            DeviceList<ForceSensor> forceSensors = body->devices<ForceSensor>();
            if(!forceSensors.empty()){
                ForceSensorVisualizerItem* forceSensorVisualizerItem= 0;
                if(j<n){
                    forceSensorVisualizerItem = dynamic_cast<ForceSensorVisualizerItem*>(restoredSubItems[j++].get());
                }else{
                    forceSensorVisualizerItem = new ForceSensorVisualizerItem();
                }
                if(forceSensorVisualizerItem){
                    forceSensorVisualizerItem->setBodyItem(bodyItem);
                    self->addSubItem(forceSensorVisualizerItem);
                    subItems.push_back(forceSensorVisualizerItem);
                }
            }

            DeviceList<RangeCamera> rangeCameras = body->devices<RangeCamera>();
            for(size_t i=0; i < rangeCameras.size(); ++i){
                PointCloudVisualizerItem* pointCloudVisualizerItem =
                        j<n ? dynamic_cast<PointCloudVisualizerItem*>(restoredSubItems[j++].get()) : new PointCloudVisualizerItem();
                if(pointCloudVisualizerItem){
                    pointCloudVisualizerItem->setBodyItem(bodyItem, rangeCameras[i]);
                    self->addSubItem(pointCloudVisualizerItem);
                    subItems.push_back(pointCloudVisualizerItem);
                }
            }

            DeviceList<RangeSensor> rangeSensors = body->devices<RangeSensor>();
            for(size_t i=0; i < rangeSensors.size(); ++i){
                RangeSensorVisualizerItem* rangeSensorVisualizerItem =
                        j<n ? dynamic_cast<RangeSensorVisualizerItem*>(restoredSubItems[j++].get()) : new RangeSensorVisualizerItem();
                if(rangeSensorVisualizerItem){
                    rangeSensorVisualizerItem->setBodyItem(bodyItem, rangeSensors[i]);
                    self->addSubItem(rangeSensorVisualizerItem);
                    subItems.push_back(rangeSensorVisualizerItem);
                }
            }

            DeviceList<Camera> cameras = body->devices<Camera>();
            for(size_t i=0; i < cameras.size(); ++i){
                if(cameras[i]->imageType()!=Camera::NO_IMAGE){
                    CameraImageVisualizerItem* cameraImageVisualizerItem =
                            j<n ? dynamic_cast<CameraImageVisualizerItem*>(restoredSubItems[j++].get()) : new CameraImageVisualizerItem();
                    if(cameraImageVisualizerItem){
                        cameraImageVisualizerItem->setBodyItem(bodyItem, cameras[i]);
                        self->addSubItem(cameraImageVisualizerItem);
                        subItems.push_back(cameraImageVisualizerItem);
                    }
                }
            }
        }

        restoredSubItems.clear();
    }
}

void SensorVisualizerItem::onDisconnectedFromRoot()
{
    for(size_t i=0; i < impl->subItems.size(); i++){
        impl->subItems[i]->detachFromParentItem();
    }
    impl->subItems.clear();
}


bool SensorVisualizerItem::store(Archive& archive)
{
    ListingPtr subItems = new Listing();

    for(size_t i=0; i < impl->subItems.size(); i++){
        Item* item = impl->subItems[i];
        string pluginName, className;
        ItemManager::getClassIdentifier(item, pluginName, className);

        ArchivePtr subArchive = new Archive();
        subArchive->write("class", className);
        subArchive->write("name", item->name());
        item->store(*subArchive);

        subItems->append(subArchive);
    }

    archive.insert("subItems", subItems);

    return true;
}


bool SensorVisualizerItem::restore(const Archive& archive)
{
    impl->restoredSubItems.clear();

    ListingPtr subItems = archive.findListing("subItems");
    if(subItems->isValid()){
        for(int i=0; i < subItems->size(); i++){
            Archive* subArchive = dynamic_cast<Archive*>(subItems->at(i)->toMapping());
            string className, itemName;
            subArchive->read("class", className);
            subArchive->read("name", itemName);

            ItemPtr item;
            item = ItemManager::create("Body", className);
            item->setName(itemName);

            item->restore(*subArchive);
            impl->restoredSubItems.push_back(item);
        }
    }
    return true;
}


ForceSensorVisualizerItem::ForceSensorVisualizerItem()
{
    scene = new SgGroup;

    SgMaterial* material = new SgMaterial;
    Vector3f color(1.0f, 0.2f, 0.2f);
    material->setDiffuseColor(Vector3f::Zero());
    material->setEmissiveColor(color);
    material->setAmbientIntensity(0.0f);
    material->setTransparency(0.6f);

    MeshGenerator meshGenerator;
    cone = new SgShape;
    cone->setMesh(meshGenerator.generateCone(0.03,  0.04));
    cone->setMaterial(material);

    cylinder = new SgShape;
    cylinder->setMesh(meshGenerator.generateCylinder(0.01, 1.0));
    cylinder->setMaterial(material);

    visualRatio = 0.002;
}


void ForceSensorVisualizerItem::setBodyItem(BodyItem* bodyItem)
{
    if(name().empty()){
        setName("ForceSensor");
    }

    connections.disconnect();
    connections.add(
        bodyItem->sigKinematicStateChanged().connect(
            [&](){ onSensorPositionsChanged(); }));

    Body* body = bodyItem->body();
    forceSensors = body->devices<ForceSensor>();
    scene->clearChildren();
    forceSensorArrows.clear();
    for(size_t i=0; i < forceSensors.size(); ++i){
        ArrowPtr arrow = new Arrow(cylinder, cone);
        forceSensorArrows.push_back(arrow);
        scene->addChild(arrow);
        connections.add(
            forceSensors[i]->sigStateChanged().connect(
                [this, i](){ updateForceSensorState(i); }));
    }

    onSensorPositionsChanged();
    updateSensorState();
}


SgNode* ForceSensorVisualizerItem::getScene()
{
    return scene;
}


void ForceSensorVisualizerItem::onSensorPositionsChanged()
{
    if(!ItemTreeView::instance()->isItemChecked(this, ItemTreeView::ID_ANY)){
        return;
    }
    for(size_t i=0; i < forceSensors.size(); ++i){
        ForceSensor* sensor = forceSensors[i];
        Vector3 p = sensor->link()->T() * sensor->localTranslation();
        forceSensorArrows[i]->setTranslation(p);
    }
}


void ForceSensorVisualizerItem::updateSensorState()
{
    if(!ItemTreeView::instance()->isItemChecked(this, ItemTreeView::ID_ANY)){
        return;
    }
    for(size_t i=0; i < forceSensors.size(); ++i){
        updateForceSensorState(i);
    }
}

    
void ForceSensorVisualizerItem::updateForceSensorState(int index)
{
    if(index < static_cast<int>(forceSensors.size())){
        ForceSensor* sensor = forceSensors[index];
        Vector3 v = sensor->link()->T() * sensor->T_local() * sensor->f();
        forceSensorArrows[index]->setVector(v * visualRatio);
    }
}


void ForceSensorVisualizerItem::doPutProperties(PutPropertyFunction& putProperty)
{
    putProperty.decimals(4)(
        _("Visual ratio"), visualRatio,
        [&](double ratio){
            if(ratio > 0.0){
                visualRatio = ratio;
                updateSensorState();
                return true;
            }
            return false;
        });
}


bool ForceSensorVisualizerItem::store(Archive& archive)
{
    archive.write("visualRatio", visualRatio);
    return true;
}


bool ForceSensorVisualizerItem::restore(const Archive& archive)
{
    archive.read("visualRatio", visualRatio);
    return true;
}


void ForceSensorVisualizerItem::setVisualRatio(double r)
{
    this->visualRatio = r;
}


PointCloudVisualizerItem::PointCloudVisualizerItem()
{

}


void PointCloudVisualizerItem::setBodyItem(BodyItem* bodyItem, RangeCamera* rangeCamera)
{
    if(name().empty()){
        setName(rangeCamera->name());
    }

    connections.disconnect();
    connections.add(
        bodyItem->sigKinematicStateChanged().connect(
            [&](){ onSensorPositionsChanged(); }));

    this->rangeCamera = rangeCamera;

    connections.add(
        rangeCamera->sigStateChanged().connect(
            [&](){ updateRangeCameraState(); }));

    onSensorPositionsChanged();
    updateRangeCameraState();
}


void PointCloudVisualizerItem::onSensorPositionsChanged()
{
    if(!ItemTreeView::instance()->isItemChecked(this, ItemTreeView::ID_ANY)){
        return;
    }

    const Affine3 T =  (rangeCamera->link()->T() * rangeCamera->T_local());
    setOffsetTransform(T);
}


void PointCloudVisualizerItem::updateRangeCameraState()
{
    if(!ItemTreeView::instance()->isItemChecked(this, ItemTreeView::ID_ANY)){
        return;
    }

    auto pointSet_ = pointSet();
    
    if(!rangeCamera->on()){
        pointSet_->clear();
    } else {
        const vector<Vector3f>& src = rangeCamera->constPoints();
        SgVertexArray& points = *pointSet_->getOrCreateVertices();
        const int numPoints = src.size();
        points.resize(numPoints);
        for(int i=0; i < numPoints; ++i){
            points[i] = src[i];
        }

        SgColorArray& colors = *pointSet_->getOrCreateColors();
        const Image& image = rangeCamera->constImage();
        if(image.empty() || image.numComponents() != 3){
            colors.clear();
        } else {
            const unsigned char* pixels = image.pixels();
            const int numPixels = image.width() * image.height();
            const int n = std::min(numPixels, numPoints);
            colors.resize(n);
            for(int i=0; i < n; ++i){
                Vector3f& c = colors[i];
                c[0] = *pixels++ / 255.0f;
                c[1] = *pixels++ / 255.0f;
                c[2] = *pixels++ / 255.0f;
            }
        }
    }
    pointSet_->notifyUpdate();
}


RangeSensorVisualizerItem::RangeSensorVisualizerItem()
{

}


void RangeSensorVisualizerItem::setBodyItem(BodyItem* bodyItem, RangeSensor* rangeSensor)
{
    if(name().empty()){
        setName(rangeSensor->name());
    }

    connections.disconnect();
    connections.add(
        bodyItem->sigKinematicStateChanged().connect(
            [&](){ onSensorPositionsChanged(); }));

    this->rangeSensor = rangeSensor;

    connections.add(
        rangeSensor->sigStateChanged().connect(
            [&](){ updateRangeSensorState(); }));

    onSensorPositionsChanged();
    updateRangeSensorState();
}


void RangeSensorVisualizerItem::onSensorPositionsChanged()
{
    if(!ItemTreeView::instance()->isItemChecked(this, ItemTreeView::ID_ANY)){
        return;
    }

    const Affine3 T = (rangeSensor->link()->T() * rangeSensor->T_local());
    setOffsetTransform(T);
}


void RangeSensorVisualizerItem::updateRangeSensorState()
{
    if(!ItemTreeView::instance()->isItemChecked(this, ItemTreeView::ID_ANY)){
        return;
    }

    auto pointSet_ = pointSet();

    if(!rangeSensor->on()){
        pointSet_->clear();
    } else {
        const RangeSensor::RangeData& src = rangeSensor->constRangeData();
        const int numPoints = src.size();
        SgVertexArray& points = *pointSet_->getOrCreateVertices();
        points.clear();

        if(!src.empty()){
            points.reserve(numPoints);
            const int numPitchSamples = rangeSensor->numPitchSamples();
            const double pitchStep = rangeSensor->pitchStep();
            const int numYawSamples = rangeSensor->numYawSamples();
            const double yawStep = rangeSensor->yawStep();

            for(int pitch=0; pitch < numPitchSamples; ++pitch){
                const double pitchAngle = pitch * pitchStep - rangeSensor->pitchRange() / 2.0;
                const double cosPitchAngle = cos(pitchAngle);
                const int srctop = pitch * numYawSamples;

                for(int yaw=0; yaw < numYawSamples; ++yaw){
                    const double distance = src[srctop + yaw];
                    if(distance <= rangeSensor->maxDistance()){
                        double yawAngle = yaw * yawStep - rangeSensor->yawRange() / 2.0;
                        float x = distance *  cosPitchAngle * sin(-yawAngle);
                        float y  = distance * sin(pitchAngle);
                        float z  = -distance * cosPitchAngle * cos(yawAngle);
                        points.push_back(Vector3f(x, y, z));
                    }
                }
            }
        }
    }
    pointSet_->notifyUpdate();
}


CameraImageVisualizerItem::CameraImageVisualizerItem()
{

}


void CameraImageVisualizerItem::setBodyItem(BodyItem* bodyItem, Camera* camera)
{
    if(name().empty()){
        string name = camera->name();
        if(dynamic_cast<RangeCamera*>(camera))
            name += "_Image";
        setName(name);
    }

    connections.disconnect();
    this->camera = camera;

    connections.add(
        camera->sigStateChanged().connect(
            [&](){ updateCameraImage(); }));

    updateCameraImage();
}


void CameraImageVisualizerItem::updateCameraImage()
{
    if(camera->on()){
        image = camera->sharedImage();
    } else {
        image.reset();
    }
    notifyImageUpdate();
}


const Image* CameraImageVisualizerItem::getImage()
{
    return image.get();
}
