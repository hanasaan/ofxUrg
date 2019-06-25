#include "ofMain.h"
#include "ofxUrg.h"

OFX_URG_BEGIN_NAMESPACE

bool Device::setupSerial(const string& device_name, int baudrate)
{
    connect_type = SERIAL;
    
    if (!device_name.empty())
    {
        device_or_ip_name = device_name;
    }
    else
    {
        device_or_ip_name = "/dev/tty.usbmodemfa131";
    }
    baudrate_or_port_number = baudrate;
    
    return open();
}

bool Device::setupEthernet(const string& ip, int port)
{
    connect_type = ETHERNET;
    device_or_ip_name = ip;
    baudrate_or_port_number = port;
    
    return open();
}

bool Device::open()
{
    if (!urg.open(device_or_ip_name.c_str(), baudrate_or_port_number,
                  (connect_type==SERIAL) ? qrk::Urg_driver::Serial : qrk::Urg_driver::Ethernet))
    {
        ofLogError("Urg connection failed") << urg.what();
        return false;
    }
    
    urg.set_scanning_parameter(minStep(), maxStep());
    urg.set_sensor_time_stamp(ofGetElapsedTimef());
    return true;
}

void Device::close() { urg.close(); }

bool Device::start()
{
    stop();
    
    qrk::Lidar::measurement_type_t measurement_type;
    switch (mode) {
        case DISTANCE:
            measurement_type = qrk::Lidar::Distance;
            break;
        case DISTANCE_INTENSITY:
            measurement_type = qrk::Lidar::Distance_intensity;
            break;
        case MULTIECHO:
            measurement_type = qrk::Lidar::Multiecho;
            break;
        case MULTIECHO_INTENSITY:
            measurement_type = qrk::Lidar::Multiecho_intensity;
            break;
    }
    
    active = urg.start_measurement(measurement_type, qrk::Urg_driver::Infinity_times, 0);
    startThread();
    return active;
}
void Device::stop()
{
    waitForThread();
    stopThread();
    urg.stop_measurement();
}

void Device::update()
{
    is_frame_new = false;
    
    lock();
    if (data != data_buffer)
    {
        data = data_buffer;
        is_frame_new = true;
    }
    
    if (mode == DISTANCE_INTENSITY || mode == MULTIECHO_INTENSITY)
    {
        if (intensity != intensity_buffer)
        {
            intensity = intensity_buffer;
            is_frame_new = true;
        }
    }
    unlock();
}

void Device::drawDebug(float width, float height) const
{
    for (int i=0; i<data.size(); i++)
    {
        float x = ofMap(i, 0, data.size(), 0, width, true);
        float y = ofMap(data[i], 0, 1500, 0, height, true);
        ofDrawLine(x, 0, x, y);
    }
}

void Device::drawDebugPolar() const
{
    ofMesh m;
    m.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
    m.addVertex(ofVec3f::zero());
    
    for (int i=0; i<data.size(); i++)
    {
        float r = data[i];
        float theta = urg.index2rad(i);
        float x = r * cos(theta);
        float y = r * sin(theta);
        m.addVertex(ofVec3f(x,y));
    }
    
    m.draw();
}

void Device::threadedFunction()
{
    while (isThreadRunning())
    {
        if (lock())
        {
            if (active)
            {
                if (mode == DISTANCE)
                {
                    if (!urg.get_distance(data_buffer, &timestamp))
                        ofLogError("urg get distance") << urg.what();
                    else
                        fps.newFrame();
                }
                if (mode == DISTANCE_INTENSITY)
                {
                    if (!urg.get_distance_intensity(data_buffer, intensity_buffer, &timestamp))
                        ofLogError("urg get distance intensity") << urg.what();
                    else
                        fps.newFrame();
                }
            }
            unlock();
        }
        ofSleepMillis(1);
    }
}


OFX_URG_END_NAMESPACE