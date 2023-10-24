////////////////////////////////////////////////////////////////////////////////
/// @file   GpioDevice.h
/// @brief  Partially abstract class for any GPIO pin(s).
///     Derived class, based on the Device interface, that implements
///     a particular type of physical device; i.e. a battery. It instantiates the specific
///     type of communication ("transport") channel to talk to its connected (IC) device.
/// @version    %Version:1 %
/// @author dv15727
/// @date   %Created:%
//
//  Copyright 2012 Bose Corporation, Framingham, MA
///////////////////////////////////////////////////////////////////////////////////

#ifndef __GpioDevice_h__
#define __GpioDevice_h__

#include "RTOS.h"
#include "Device.h"
#include "NotifyTargetTaskIF.h"
#include <string>
#include <queue>

// GRRR! None of these are accessible outside of kernel-mode device drivers!
//#include <linux/ioport.h>         // for IORESOURCE_IRQ_HIGHEDGE, IORESOURCE_IRQ_LOWEDGE
//#include <linux/irq.h>            // for IRQ_TYPE_EDGE_RISING, IRQ_TYPE_EDGE_FALLING
//#include <linux/interrupt.h>        // IRQF_TRIGGER_RISING, IRQF_TRIGGER_FALLING
#ifndef IRQF_TRIGGER_NONE
#define IRQF_TRIGGER_NONE   0x00000000
#endif  // IRQF_TRIGGER_NONE
#ifndef IRQF_TRIGGER_RISING
#define IRQF_TRIGGER_RISING 0x00000001
#endif // IRQF_TRIGGER_RISING
#ifndef IRQF_TRIGGER_FALLING
#define IRQF_TRIGGER_FALLING    0x00000002
#endif // IRQF_TRIGGER_FALLING

using std::string;
using std::queue;

#define GPIO_VALUE_NOT_SET  0xff
#define GPIO_VALUE_BUFSIZE  128

class NotifyTargetTaskIF;

///////////////////////////////////////////////////////////////////////////////
/// GpioDevice - derived class, based on the Device interface, that implements
/// a particular type of physical device; i.e. a battery. It instantiates the specific
/// type of communication ("transport") channel to talk to its connected (IC) device.
///////////////////////////////////////////////////////////////////////////////
class GpioDevice : public Device
{
public:
    GpioDevice( const char* DeviceName, NotifyTargetTaskIF* pOwner = NULL, bool Blocking = true,
                bool InterruptDriven = true, bool debug = false,
                SINT32 edge = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING ); // edge-triggered, both edges
    virtual ~GpioDevice();


protected:
    // START what you need for one single "/dev/whatever" callback; dupe this for ea. one
    CallbackArg mCallbackArg;
    virtual bool Callback( CallbackArg* pArg );
    BObserver<GpioDevice, CallbackArg>* mpObserver;
    // END what you need for one single "/dev/whatever" callback

public:
    // Accessors
    virtual DeviceReturn_t Read( void*, UINT32 );
    virtual DeviceReturn_t Write( const void*, UINT32 );
    virtual SINT32 Level();
    virtual void Close();
    void Reopen();

    bool IsOpen()
    {
        return mIsOpen;
    }

    virtual size_t size()
    {
        return mValue.size();
    }
    virtual bool empty()
    {
        return mValue.empty();
    }
    virtual void clear();
    virtual UINT8& front()
    {
        return mValue.front();
    }
    virtual UINT8& back()
    {
        return mValue.back();
    }
    virtual void pop()
    {
        mValue.pop();
    }
    virtual void push( const UINT8& v )
    {
        mValue.push( v );
    }
    virtual size_t LastReadSize()
    {
        return mLastReadSize;
    }
    // so clients can chain a callback to our callback
    virtual DeviceReturn_t RegisterCallback( BAbstractObserver* pObserver,
                                             BNotification* pCallbackArg );
    virtual DeviceReturn_t RemoveCallback( BAbstractObserver* pObserver );

protected:
    virtual void ConfigGpioSignal();
    NotifyTargetTaskIF::CallbackEntryPtr mpClientCallback;

protected:
    string mDeviceName;
    // this flag is set when the INPUT device is enabled to be edge-triggered in the kernel
    // driver, s.t. the kernel reads its state and posts it in the 'read()' queue whenever
    // the line(s) changes state.
    bool mInterruptDriven;
    bool mBlocking;
    NotifyTargetTaskIF* mpOwner;
    SINT32 mFd;     // the descriptor of the open "/dev/whatever" interface
    queue<UINT8> mValue;
    size_t mLastReadSize;
    bool mIsOpen;
    SINT32 mEdge;
private:
    // Disable Copies
    GpioDevice( const GpioDevice & source );
    GpioDevice operator=( const GpioDevice & source );
};

#endif // __GpioDevice_h__
