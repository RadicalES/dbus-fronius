dbus-fronius
============
This application reads data from Fronius photoelectric voltage inverters, and
publishes it on the D-Bus. It is designed to run on the Color Control (CCGX).

The color control gui application contains several pages to change the behaviour
of this application, especially settings on how to detect the inverters.

In order to build the application for the color control you need a linux system,
a recent version of QT creator and the CCGX SDK. You can find the SDK here:

https://www.victronenergy.com/live/open_source:ccgx:setup_development_environment

This page also contains information on how to configure QT creator to use the CCGX cross compiler.
Next you can load the project file software/dbus-fronius.pro in QT creator and create the
CCGX binary.

Compatibility
=============

All Fronius inverters supporting the solar API v1. Also the sunspec modbus TCP standard is
supported. If modbus TCP is enabled on the Fronius using the web interface, it will be used.
Note that this is required for power limiting.

Other PV inverters that implement the sunspec standard are supported. This has been tested with
a SMA sunnyboy inverter. Results with other inverters may vary. At this moment, it is assumed that
the sunspec registers are available at unit ID 126. There is no way to change this.

Sunspec quirks
--------------

Fronius:
The Fronius inverter appears to be compliant with the sunspec standard regarding the registers
needed in this project. However, there may be multiple PV inverters sharing a single IP address.
Multiple inverters are connected to a data manager, and not directly to the network. The data
manager is connected to the network, and acts as a gateway for modbus TCP communication. Each
inverter has its own unit ID. The unit IDs are retrieved using the solar API.

SMA sunny boy:
* The Operating State (part of the inverter model) is not supported, allthough the SunSpec standard
specifies it as mandatory. This is a problem, because the power limiter in hub4control uses this
state.
* The parameters controlling power limiting are write only (SunSpec has them read/write). This is a
problem because it is impossible to retrieve their current value, for example during startup. There
is a workaround possible, but it is not part of this release.

Testing on a linux PC
=====================

To compile and run on a (linux) PC you will also need a QT SDK (version 4.8.x), including QT D-Bus 
support. Because you do not have access to the system D-Bus (unless you run as root or adjust the
D-Bus configuration) you should start the fronius application with: 'dbus-fronius --dbus session'
Note that QT for windows does not support D-Bus, so you cannot build a windows executable.

The dbus-fronius executable expects the CCGX settings manager (localsettings) to be running.
localsettings is available on github:

https://github.com/victronenergy/localsettings

The README.md of localsettings contains some information on how to run localsettings on your PC.

Unit tests
==========

In order to run the unit tests, you need to install a python interpreter (v2.7 or newer).

Architecture
============

The application consists of 3 layers:
  * Data acquisition layer:
    - `FroniusSolarAPI` implements the http+json protocol used to extract data from the inverters.
    - `ModbusTcpClient` used to communicate with SunSpec PV inverters.
    - `InverterGateway` is reponsible for device detection. This actual detection is delegated to
      one of the `AbstractDetector` classes. There is one for the Solar API (`SolarApiDetector`),
      and one for SunSpec (`SunspecDetector`). PV inverters are found by sending Solar API/
      Modbus requests out to all IP addresses in the network (the maximum number of IP addresses is
      limited). IP addresses where a PV inverter has already been detected take priority. This is
      a tedious procedure which causes a lot of network travel. However, it is necessary for auto
      detection, because the inverters do not support any protocol for efficient detection (like
      upnp).
    - `InverterMediator` Each mediator represents a PV inverter found while the service is running.
      If a inverter is detected by a `InverterGateway`, the mediator will know whether the detected
      inverter is the one he represents. It is reponsible for creating and removing `Inverter`
      objects which publishes the inverter data on the D-Bus, and for starting/stopping
      communication with the inverters.
    - `DBusFronius` Ties everything together. It creates 2 inverter `InverteGateway` objects for
      device detection. If a device is found, it will create an `InverterMediator` if there is no
      mediator yet which represents the inverter. `DBusFronius` will also publish the
      `com.victronenergy.fronius` service.
  * Data model
    - `Settings` Persistent inverter independent settings (stored on `com.victronenergy.settings`),
      such as the list of IP addresses where data cards have been found. This class takes the
      relevant data from the D-Bus using VeQItems.
    - `InverterSettings` Persistent inverter settings.
    - `Inverter` contains all values published on the D-Bus.
