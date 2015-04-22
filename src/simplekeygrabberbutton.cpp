//#include <QDebug>

#include "simplekeygrabberbutton.h"
#include "event.h"
#include "antkeymapper.h"
#include "eventhandlerfactory.h"

#ifdef Q_OS_WIN
#include "winextras.h"
#endif

#ifdef Q_OS_UNIX
    #if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QApplication>
    #endif
#endif

#ifdef Q_OS_UNIX

    #if defined(WITH_UINPUT) && defined(WITH_X11)
        #include "qtx11keymapper.h"

        static QtX11KeyMapper x11KeyMapper;
    #endif
#elif defined(Q_OS_WIN)
    static QtWinKeyMapper nativeWinKeyMapper;
#endif

SimpleKeyGrabberButton::SimpleKeyGrabberButton(QWidget *parent) :
    QPushButton(parent)
{
    grabNextAction = false;
    grabbingWheel = false;
    edited = false;
    this->installEventFilter(this);
}

void SimpleKeyGrabberButton::keyPressEvent(QKeyEvent *event)
{
    // Do not allow closing of dialog using Escape key
    if (event->key() == Qt::Key_Escape)
    {
        return;
    }

    QPushButton::keyPressEvent(event);
}

bool SimpleKeyGrabberButton::eventFilter(QObject *obj, QEvent *event)
{
    Q_UNUSED(obj);

    int controlcode = 0;
    if (grabNextAction && event->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent *mouseEve = (QMouseEvent*) event;
        if (mouseEve->button() == Qt::RightButton)
        {
            controlcode = 3;
        }
        else if (mouseEve->button() == Qt::MiddleButton)
        {
            controlcode = 2;
        }
        else {
            controlcode = mouseEve->button();
        }

        //setText(QString(tr("Mouse")).append(" ").append(QString::number(controlcode)));

        buttonslot.setSlotCode(controlcode);
        buttonslot.setSlotMode(JoyButtonSlot::JoyMouseButton);
        refreshButtonLabel();
        edited = true;
        releaseMouse();
        releaseKeyboard();

        grabNextAction = grabbingWheel = false;
        emit buttonCodeChanged(controlcode);
    }
    else if (grabNextAction && event->type() == QEvent::KeyRelease)
    {
        QKeyEvent *keyEve = static_cast<QKeyEvent*>(event);
        int tempcode = keyEve->nativeScanCode();
        int virtualactual = keyEve->nativeVirtualKey();

        BaseEventHandler *handler = EventHandlerFactory::getInstance()->handler();

#ifdef Q_OS_WIN
        int finalvirtual = 0;
        int checkalias = 0;

  #ifdef WITH_VMULTI
        if (handler->getIdentifier() == "vmulti")
        {
            finalvirtual = WinExtras::correctVirtualKey(controlcode, virtualactual);
            checkalias = AntKeyMapper::getInstance()->returnQtKey(finalvirtual);

            unsigned int tempQtKey = nativeWinKeyMapper.returnQtKey(finalvirtual);
            if (tempQtKey > 0)
            {
                finalvirtual = AntKeyMapper::getInstance()->returnVirtualKey(tempQtKey);
                checkalias = AntKeyMapper::getInstance()->returnQtKey(finalvirtual);
            }
            else
            {
                finalvirtual = AntKeyMapper::getInstance()->returnVirtualKey(keyEve->key());
            }
        }

  #endif

        if (handler->getIdentifier() == "sendinput")
        {
            // Find more specific virtual key (VK_SHIFT -> VK_LSHIFT)
            // by checking for extended bit in scan code.
            finalvirtual = WinExtras::correctVirtualKey(controlcode, virtualactual);
            checkalias = AntKeyMapper::getInstance()->returnQtKey(finalvirtual, controlcode);
        }

#else

    #if defined(WITH_X11)
        int finalvirtual = 0;
        int checkalias = 0;

        #if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
        if (QApplication::platformName() == QStringLiteral("xcb"))
        {
        #endif
        // Obtain group 1 X11 keysym. Removes effects from modifiers.
        finalvirtual = X11KeyCodeToX11KeySym(tempcode);

        #ifdef WITH_UINPUT
        if (handler->getIdentifier() == "uinput")
        {
            // Find Qt Key corresponding to X11 KeySym.
            checkalias = x11KeyMapper.returnQtKey(finalvirtual);
            // Find corresponding Linux input key for the Qt key.
            finalvirtual = AntKeyMapper::getInstance()->returnVirtualKey(checkalias);
        }
        #endif

        #ifdef WITH_XTEST
        if (handler->getIdentifier() == "xtest")
        {
            // Check for alias against group 1 keysym.
            checkalias = AntKeyMapper::getInstance()->returnQtKey(finalvirtual);
        }
        #endif

        #if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
        }
        else
        {
            // Not running on xcb platform.
            finalvirtual = controlcode;
            checkalias = AntKeyMapper::getInstance()->returnQtKey(finalvirtual);
        }
        #endif

    #else
        int finalvirtual = 0;
        int checkalias = 0;
        #if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
        if (QApplication::platformName() == QStringLiteral("xcb"))
        {
        #endif
        finalvirtual = AntKeyMapper::getInstance()->returnVirtualKey(keyEve->key());
        checkalias = AntKeyMapper::getInstance()->returnQtKey(finalvirtual);
        #if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
        }
        else
        {
            // Not running on xcb platform.
            finalvirtual = controlcode;
            checkalias = AntKeyMapper::getInstance()->returnQtKey(finalvirtual);
        }
        #endif

    #endif

#endif

        controlcode = tempcode;
        bool valueUpdated = false;

        if ((keyEve->modifiers() & Qt::ControlModifier) && keyEve->key() == Qt::Key_X)
        {
            controlcode = 0;
            refreshButtonLabel();
        }
        else if (controlcode <= 0)
        {
            controlcode = 0;
            setText("");
            valueUpdated = true;
            edited = true;
        }
        else
        {
            if (checkalias > 0)
            {
                buttonslot.setSlotCode(finalvirtual, checkalias);
                buttonslot.setSlotMode(JoyButtonSlot::JoyKeyboard);
                setText(keysymToKeyString(finalvirtual, checkalias).toUpper());
            }
            else
            {
                buttonslot.setSlotCode(virtualactual);
                buttonslot.setSlotMode(JoyButtonSlot::JoyKeyboard);
                setText(keysymToKeyString(virtualactual).toUpper());
            }

            edited = true;
            valueUpdated = true;
        }

        grabNextAction = false;
        grabbingWheel = false;
        releaseMouse();
        releaseKeyboard();

        if (valueUpdated)
        {
            emit buttonCodeChanged(controlcode);
        }
    }
    else if (grabNextAction && event->type() == QEvent::Wheel && !grabbingWheel)
    {
        grabbingWheel = true;
    }
    else if (grabNextAction && event->type() == QEvent::Wheel)
    {
        QWheelEvent *wheelEve = (QWheelEvent*) event;
        QString text = QString(tr("Mouse")).append(" ");

        if (wheelEve->orientation() == Qt::Vertical && wheelEve->delta() >= 120)
        {
            controlcode = 4;
        }
        else if (wheelEve->orientation() == Qt::Vertical && wheelEve->delta() <= -120)
        {
            controlcode = 5;
        }
        else if (wheelEve->orientation() == Qt::Horizontal && wheelEve->delta() >= 120)
        {
            controlcode = 6;
        }
        else if (wheelEve->orientation() == Qt::Horizontal && wheelEve->delta() <= -120)
        {
            controlcode = 7;
        }

        if (controlcode > 0)
        {
            text = text.append(QString::number(controlcode));
            setText(text);

            grabNextAction = false;
            grabbingWheel = false;
            edited = true;
            releaseMouse();
            releaseKeyboard();
            buttonslot.setSlotCode(controlcode);
            buttonslot.setSlotMode(JoyButtonSlot::JoyMouseButton);
            emit buttonCodeChanged(controlcode);
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent *mouseEve = (QMouseEvent*) event;
        if (mouseEve->button() == Qt::LeftButton)
        {
            grabNextAction = true;
            setText("...");
            setFocus();
            grabKeyboard();
            grabMouse();
        }
    }


    return false;
}

void SimpleKeyGrabberButton::setValue(int value, unsigned int alias, JoyButtonSlot::JoySlotInputAction mode)
{
    buttonslot.setSlotCode(value, alias);
    buttonslot.setSlotMode(mode);
    edited = true;

    setText(buttonslot.getSlotString());
}

void SimpleKeyGrabberButton::setValue(int value, JoyButtonSlot::JoySlotInputAction mode)
{
    buttonslot.setSlotCode(value);
    buttonslot.setSlotMode(mode);
    edited = true;

    setText(buttonslot.getSlotString());
}

void SimpleKeyGrabberButton::setValue(QString value, JoyButtonSlot::JoySlotInputAction mode)
{
    switch (mode)
    {
        case JoyButtonSlot::JoyLoadProfile:
        {
            buttonslot.setTextData(value);
            buttonslot.setSlotMode(mode);
            edited = true;
            break;
        }
    }

    setText(buttonslot.getSlotString());
}

JoyButtonSlot* SimpleKeyGrabberButton::getValue()
{
    return &buttonslot;
}

void SimpleKeyGrabberButton::refreshButtonLabel()
{
    setText(buttonslot.getSlotString());
}

bool SimpleKeyGrabberButton::isEdited()
{
    return edited;
}
