//=====================================================================
// Copyright 2019 (c), Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//=====================================================================

#include <QApplication>

#include "Compressonator.h"
#include "Common.h"
#include "cpMainComponents.h"
#include "PluginManager.h"


#define MSG_HANDLER

// Our Static Plugin Interfaces
#pragma comment(lib,"ASTC.lib")
#pragma comment(lib,"EXR.lib")
#pragma comment(lib,"KTX.lib")
#pragma comment(lib,"TGA.lib")
#pragma comment(lib,"IMGAnalysis.lib")

#ifdef USE_CRN
#pragma comment(lib,"CRN.lib")
#endif


extern void *make_Plugin_ASTC();
extern void *make_Plugin_EXR();
extern void *make_Plugin_KTX();
extern void *make_Plugin_TGA();
extern void *make_Plugin_CAnalysis();

#ifdef USE_CRN
extern void *make_Plugin_CRN();
#endif

// Setup Static Host Pluging Libs
extern void CMP_RegisterHostPlugins();

#define SEPERATOR_STYLE "QMainWindow::separator { background-color: #d7d6d5; width: 3px; height: 3px; border:none; }"
#define PERCENTAGE_OF_MONITOR_WIDTH_FOR_SCREEN  0.65
#define PERCENTAGE_OF_MONITOR_HEIGHT_FOR_SCREEN 0.8

extern PluginManager   g_pluginManager;
bool            g_bAbortCompression;
CMIPS*          g_CMIPS;                                // Global MIPS functions shared between app and all IMAGE plugins 
CMIPS*          g_GUI_CMIPS;                            // Global MIPS functions shared by 3DModels


void GetSupportedFileFormats(QList<QByteArray> &g_supportedFormats)
{

    // Assemble list of supported Image Formats from our plugin
    int numPlugins = g_pluginManager.getNumPlugins();
    for (int i = 0; i< numPlugins; i++)
    {
        if (strcmp(g_pluginManager.getPluginType(i), "IMAGE") == 0)
        {
            QByteArray bArray = g_pluginManager.getPluginName(i);
#ifdef _WIN32
            QByteArray fformat = bArray.toUpper();
#else
            QByteArray fformat = bArray.toLower();
#endif
            if (fformat == "ANALYSIS") continue;
            if (!g_supportedFormats.contains(fformat))
                g_supportedFormats.append(fformat);
        }
        else
        if (strcmp(g_pluginManager.getPluginType(i), "3DMODEL_LOADER") == 0)
        {
            QByteArray bArray = g_pluginManager.getPluginName(i);
#ifdef _WIN32
            QByteArray fformat = bArray.toUpper();
#else
            QByteArray fformat = bArray.toLower();
#endif
            if (!g_supportedFormats.contains(fformat))
                g_supportedFormats.append(fformat);
        }
    }

    // Get a list of all Supported file formats from Qt Plugins
    QList<QByteArray> QtFormats = QImageReader::supportedImageFormats();

    // Upppercase List
    QList<QByteArray>::Iterator i;
    for (i = QtFormats.begin(); i != QtFormats.end(); ++i)
    {
        QByteArray fformat = (*i);
        fformat = fformat.toUpper();
        if (!g_supportedFormats.contains(fformat))
            g_supportedFormats.append(fformat);
    }
    
    // Sort the list to alphabetical order
    std::sort(g_supportedFormats.begin(), g_supportedFormats.end());

}

//----------------------------------------------------------
// Compressonator Lib codec overrides
//----------------------------------------------------------

//----------------- GTC: Run Time Encoder ------------------
#ifdef USE_GTC
PluginInterface_Encoder     *g_plugin_EncoderGTC   = NULL;
CMP_Encoder                 *g_Codec_GTC = NULL;
extern void(*GTC_DecompressBlock)(void *out, void *in);
extern void(*GTC_CompressBlock)(void * srcblock, void *dest, void *blockoptions);
void g_GTC_DecompressBlock(void *in, void *out)
{
if (g_Codec_GTC)
    g_Codec_GTC->DecompressBlock(in, out);
}

void g_GTC_CompressBlock(void *in, void *out, void *blockoptions)
{
    if (g_Codec_GTC)
    {
        g_Codec_GTC->CompressBlock(in, out, blockoptions);
    }
}
#endif

//----------------- BASIS: Run Time Encoder ------------------
#ifdef USE_BASIS
PluginInterface_Encoder     *g_plugin_EncoderBASIS = NULL;
CMP_Encoder                 *g_Codec_BASIS = NULL;
extern int(*BASIS_CompressTexture)(void * in, void *out, void *blockoptions);
extern int(*BASIS_DecompressTexture)(void * in, void *out, void *blockoptions);

int g_BASIS_CompressTexture(void *in, void *out, void *blockoptions)
{
    if (g_Codec_BASIS)
    {
        return g_Codec_BASIS->CompressTexture(in, out, blockoptions);
    }
    return 0;
}

int g_BASIS_DecompressTexture(void *in, void *out, void *blockoptions)
{
    if (g_Codec_BASIS)
    {
        return g_Codec_BASIS->DecompressTexture(in, out, blockoptions);
    }
    return 0;
}
#endif


int main(int argc, char **argv)
{


    try
    {
        QApplication app(argc, argv);
        QString dirPath = QApplication::applicationDirPath();
        QApplication::addLibraryPath(dirPath + "./plugins/platforms/");
        QApplication::addLibraryPath(dirPath + "./plugins/");

        app.setWindowIcon(QIcon(":/CompressonatorGUI/Images/acompress-256.png"));

        // ==========================
        // Mip Settings Class
        // ==========================
        g_GUI_CMIPS =  new CMIPS;
        g_CMIPS     = new CMIPS;

        const QIcon iconPixMap(":/CompressonatorGUI/Images/compress.png");
        const QString ProductName = "Compressonator";

        //----------------------------------
        // Load plugin List for processing
        //----------------------------------
        g_pluginManager.registerStaticPlugin("IMAGE",  "ASTC",      (void*) make_Plugin_ASTC);
        g_pluginManager.registerStaticPlugin("IMAGE",  "EXR",       (void*) make_Plugin_EXR);
        g_pluginManager.registerStaticPlugin("IMAGE",  "KTX",       (void*) make_Plugin_KTX);

#ifdef USE_CRN
        g_pluginManager.registerStaticPlugin("IMAGE", "CRN", make_Plugin_CRN);
#endif

        // TGA is supported by Qt to some extent if it fails we will try to load it using our custom code
        g_pluginManager.registerStaticPlugin("IMAGE",  "TGA",       (void*) make_Plugin_TGA);
        g_pluginManager.registerStaticPlugin("IMAGE", "ANALYSIS",   (void*) make_Plugin_CAnalysis);

        g_pluginManager.getPluginList("/plugins",true);
        CMP_RegisterHostPlugins();
        g_bAbortCompression = false;

        //---------------------------------------
        // attempt to load compute GTC Codec
        //---------------------------------------
#ifdef USE_GTC
        g_plugin_EncoderGTC = reinterpret_cast<PluginInterface_Encoder *>(g_pluginManager.GetPlugin("ENCODER", "GTC"));
        // Found GTC Codec
        if (g_plugin_EncoderGTC)
        {
            //-------------------------------
            // create the compression  Codec
            //-------------------------------
            g_Codec_GTC = (CMP_Encoder*)g_plugin_EncoderGTC->TC_Create();

            //------------------------------------------------------------
            // Assign compressonator lib GTC codec to Compute GTC Codec
            //------------------------------------------------------------
            if (g_Codec_GTC)
            {
                GTC_CompressBlock   = g_GTC_CompressBlock;
                GTC_DecompressBlock = g_GTC_DecompressBlock;
            }
        }
#endif

#ifdef USE_BASIS
        //---------------------------------------
        // attempt to load compute BASIS Codec
        //---------------------------------------
        g_plugin_EncoderBASIS = reinterpret_cast<PluginInterface_Encoder *>(g_pluginManager.GetPlugin("ENCODER", "BASIS"));
        // Found BASIS Codec
        if (g_plugin_EncoderBASIS)
        {
            //-------------------------------
            // create the compression  Codec
            //-------------------------------
            g_Codec_BASIS = (CMP_Encoder*)g_plugin_EncoderBASIS->TC_Create();

            // ToDo: Assignment to new encoder interfaces
            if (g_Codec_BASIS)
            {
                BASIS_CompressTexture   = g_BASIS_CompressTexture;
                BASIS_DecompressTexture = g_BASIS_DecompressTexture;
            }
        }
#endif

        cpMainComponents mainComponents(NULL,NULL);

#ifdef MSG_HANDLER
        PrintStatusLine = &mainComponents.PrintStatus;
        qInstallMessageHandler((QtMessageHandler) mainComponents.msgHandler);
#endif
        QDesktopWidget *desktop = new QDesktopWidget();
        mainComponents.resize(desktop->screenGeometry().width()*PERCENTAGE_OF_MONITOR_WIDTH_FOR_SCREEN, desktop->screenGeometry().height()*PERCENTAGE_OF_MONITOR_HEIGHT_FOR_SCREEN);
        mainComponents.show();

        app.setStyleSheet(SEPERATOR_STYLE);

        int ret = app.exec();

        delete g_GUI_CMIPS;
        delete g_CMIPS;

#ifdef USE_GTC
        //------------------------------------------
        // Cleanup the compute GTC compression Codec
        //------------------------------------------
        if (g_plugin_EncoderGTC)
        {
            if (g_Codec_GTC)
                g_plugin_EncoderGTC->TC_Destroy(g_Codec_GTC);
            delete  g_plugin_EncoderGTC;
        }
#endif

#ifdef USE_BASIS
        //------------------------------------------
        // Cleanup the compute compression Codec
        //------------------------------------------
        if (g_plugin_EncoderBASIS)
        {
            if (g_Codec_BASIS)
                g_plugin_EncoderBASIS->TC_Destroy(g_Codec_BASIS);
            delete  g_plugin_EncoderBASIS;
        }
#endif

        return ret;

    }
    catch (std::exception &e)
    {
        qDebug() << e.what();
    }
    catch (...)
    {
        qDebug() << "Unknown Error";
    }
    return(-1);
}
