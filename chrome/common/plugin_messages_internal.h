// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/native_widget_types.h"
#include "base/shared_memory.h"
#include "build/build_config.h"
#include "ipc/ipc_message_macros.h"
#include "webkit/glue/webcursor.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

//-----------------------------------------------------------------------------
// PluginProcess messages
// These are messages sent from the browser to the plugin process.
IPC_BEGIN_MESSAGES(PluginProcess)
  // Tells the plugin process to create a new channel for communication with a
  // given renderer.  The channel name is returned in a
  // PluginProcessHostMsg_ChannelCreated message.  The renderer ID is passed so
  // that the plugin process reuses an existing channel to that process if it
  // exists. This ID is a unique opaque identifier generated by the browser
  // process.
  IPC_MESSAGE_CONTROL2(PluginProcessMsg_CreateChannel,
                       int /* renderer_id */,
                       bool /* off_the_record */)

  // Allows a chrome plugin loaded in the browser process to send arbitrary
  // data to an instance of the same plugin loaded in a plugin process.
  IPC_MESSAGE_CONTROL1(PluginProcessMsg_PluginMessage,
                       std::vector<uint8> /* opaque data */)

  // The following messages are used by all child processes, even though they
  // are listed under PluginProcess.  It seems overkill to define ChildProcess.
  // Tells the child process it should stop.
  IPC_MESSAGE_CONTROL0(PluginProcessMsg_AskBeforeShutdown)

  // Sent in response to PluginProcessHostMsg_ShutdownRequest to tell the child
  // process that it's safe to shutdown.
  IPC_MESSAGE_CONTROL0(PluginProcessMsg_Shutdown)

#if defined(IPC_MESSAGE_LOG_ENABLED)
  // Tell the child process to begin or end IPC message logging.
  // Like above, this is used by all ChildProcesses.
  IPC_MESSAGE_CONTROL1(PluginProcessMsg_SetIPCLoggingEnabled,
                       bool /* on or off */)
#endif

#if defined(OS_MACOSX)
  // Notifies a plugin process that keyboard focus has changed.  If another
  // plugin instance has received focus, the instance IDs is passed as a
  // parameter; if focus has been taken away from a plugin, 0 is passed.
  IPC_MESSAGE_CONTROL1(PluginProcessMsg_PluginFocusNotify,
                       uint32 /* instance ID */)
#endif

IPC_END_MESSAGES(PluginProcess)


//-----------------------------------------------------------------------------
// PluginProcessHost messages
// These are messages sent from the plugin process to the browser process.
IPC_BEGIN_MESSAGES(PluginProcessHost)
  // Response to a PluginProcessMsg_CreateChannel message.
  IPC_MESSAGE_CONTROL1(PluginProcessHostMsg_ChannelCreated,
                       IPC::ChannelHandle /* channel_handle */)

  IPC_SYNC_MESSAGE_CONTROL0_1(PluginProcessHostMsg_GetPluginFinderUrl,
                              std::string /* plugin finder URL */)

  IPC_MESSAGE_CONTROL0(PluginProcessHostMsg_ShutdownRequest)

  // Allows a chrome plugin loaded in a plugin process to send arbitrary
  // data to an instance of the same plugin loaded in the browser process.
  IPC_MESSAGE_CONTROL1(PluginProcessHostMsg_PluginMessage,
                       std::vector<uint8> /* opaque data */)

  // Allows a chrome plugin loaded in a plugin process to send arbitrary
  // data to an instance of the same plugin loaded in the browser process.
  IPC_SYNC_MESSAGE_CONTROL1_1(PluginProcessHostMsg_PluginSyncMessage,
                              std::vector<uint8> /* opaque data */,
                              std::vector<uint8> /* opaque data response */)

  // Used to get cookies for the given URL.  The request_context is a
  // CPBrowsingContext, but is passed as int32 to avoid compilation errors.
  IPC_SYNC_MESSAGE_CONTROL2_1(PluginProcessHostMsg_GetCookies,
                              int32 /* request_context */,
                              GURL /* url */,
                              std::string /* cookies */)

  // Used by the plugin process to verify that its renderer |renderer_id| has
  // permission to access the given |files|.
  IPC_SYNC_MESSAGE_CONTROL2_1(PluginProcessHostMsg_AccessFiles,
                              int /* renderer_id */,
                              std::vector<std::string> /* files */,
                              bool /* allowed */)

  // Get the list of proxies to use for |url|, as a semicolon delimited list
  // of "<TYPE> <HOST>:<PORT>" | "DIRECT". See also ViewHostMsg_ResolveProxy
  // which does the same thing.
  IPC_SYNC_MESSAGE_CONTROL1_2(PluginProcessHostMsg_ResolveProxy,
                              GURL /* url */,
                              int /* network error */,
                              std::string /* proxy list */)

#if defined(OS_WIN)
  // Creates a child window of the given parent window on the UI thread.
  IPC_SYNC_MESSAGE_CONTROL1_1(PluginProcessHostMsg_CreateWindow,
                              HWND /* parent */,
                              HWND /* child */)

  // Destroys the given window's parent on the UI thread.
  IPC_MESSAGE_CONTROL2(PluginProcessHostMsg_PluginWindowDestroyed,
                       HWND /* window */,
                       HWND /* parent */)

  IPC_MESSAGE_ROUTED3(PluginProcessHostMsg_DownloadUrl,
                      std::string /* URL */,
                      int /* process id */,
                      HWND /* caller window */)
#endif

#if defined(OS_LINUX)
  // On Linux, the mapping between NativeViewId and X window ids
  // is known only to the browser.  This message lets the plugin process
  // ask about a NativeViewId that was provided by the renderer.
  // It will get 0 back if it's a bogus input.
  IPC_SYNC_MESSAGE_CONTROL1_1(PluginProcessHostMsg_MapNativeViewId,
                             gfx::NativeViewId /* input: native view id */,
                             gfx::PluginWindowHandle /* output: X window id */)
#endif

#if defined(OS_MACOSX)
  // On Mac OS X, we need the browser to keep track of plugin windows so
  // that it can add and remove them from stacking groups, hide and show the
  // menu bar, etc.  We pass the window rect for convenience so that the
  // browser can easily tell if the window is fullscreen.

  // Notifies the browser that the plugin has selected a window (i.e., brought
  // it to the front and wants it to have keyboard focus).
  IPC_MESSAGE_CONTROL3(PluginProcessHostMsg_PluginSelectWindow,
                       uint32 /* window ID */,
                       gfx::Rect /* window rect */,
                       bool /* modal */)

  // Notifies the browser that the plugin has shown a window.
  IPC_MESSAGE_CONTROL3(PluginProcessHostMsg_PluginShowWindow,
                       uint32 /* window ID */,
                       gfx::Rect /* window rect */,
                       bool /* modal */)

  // Notifies the browser that the plugin has hidden a window.
  IPC_MESSAGE_CONTROL2(PluginProcessHostMsg_PluginHideWindow,
                       uint32 /* window ID */,
                       gfx::Rect /* window rect */)

  // Notifies the browser that a plugin instance has received keyboard focus.
  IPC_MESSAGE_CONTROL2(PluginProcessHostMsg_PluginReceivedFocus,
                       uint32 /* process ID */,
                       uint32 /* instance ID */)

  // Notifies the browser that a plugin instance has requested a cursor
  // visibility change.
  IPC_MESSAGE_CONTROL1(PluginProcessHostMsg_PluginSetCursorVisibility,
                       bool /* cursor visibility */)
#endif

IPC_END_MESSAGES(PluginProcessHost)


//-----------------------------------------------------------------------------
// Plugin messages
// These are messages sent from the renderer process to the plugin process.
IPC_BEGIN_MESSAGES(Plugin)
  // Tells the plugin process to create a new plugin instance with the given
  // id.  A corresponding WebPluginDelegateStub is created which hosts the
  // WebPluginDelegateImpl.
  IPC_SYNC_MESSAGE_CONTROL1_1(PluginMsg_CreateInstance,
                              std::string /* mime_type */,
                              int /* instance_id */)

  // The WebPluginDelegateProxy sends this to the WebPluginDelegateStub in its
  // destructor, so that the stub deletes the actual WebPluginDelegateImpl
  // object that it's hosting.
  IPC_SYNC_MESSAGE_CONTROL1_0(PluginMsg_DestroyInstance,
                              int /* instance_id */)

  IPC_SYNC_MESSAGE_CONTROL0_1(PluginMsg_GenerateRouteID,
                             int /* id */)

  // The messages below all map to WebPluginDelegate methods.
  IPC_SYNC_MESSAGE_ROUTED1_1(PluginMsg_Init,
                             PluginMsg_Init_Params,
                             bool /* result */)

  // Used to synchronously request a paint for windowless plugins.
  IPC_SYNC_MESSAGE_ROUTED1_0(PluginMsg_Paint,
                             gfx::Rect /* damaged_rect */)

  // Sent by the renderer after it paints from its backing store so that the
  // plugin knows it can send more invalidates.
  IPC_MESSAGE_ROUTED0(PluginMsg_DidPaint)

  IPC_SYNC_MESSAGE_ROUTED0_2(PluginMsg_Print,
                             base::SharedMemoryHandle /* shared_memory*/,
                             size_t /* size */)

  IPC_SYNC_MESSAGE_ROUTED0_1(PluginMsg_GetPluginScriptableObject,
                             int /* route_id */)

  IPC_MESSAGE_ROUTED3(PluginMsg_DidFinishLoadWithReason,
                      GURL /* url */,
                      int /* reason */,
                      int /* notify_id */)

  // Updates the plugin location.
  IPC_MESSAGE_ROUTED1(PluginMsg_UpdateGeometry,
                      PluginMsg_UpdateGeometry_Param)

  // A synchronous version of above.
  IPC_SYNC_MESSAGE_ROUTED1_0(PluginMsg_UpdateGeometrySync,
                             PluginMsg_UpdateGeometry_Param)

  IPC_SYNC_MESSAGE_ROUTED0_0(PluginMsg_SetFocus)

  IPC_SYNC_MESSAGE_ROUTED1_2(PluginMsg_HandleInputEvent,
                             IPC::WebInputEventPointer /* event */,
                             bool /* handled */,
                             WebCursor /* cursor type*/)

#if defined(OS_MACOSX)
  IPC_MESSAGE_ROUTED1(PluginMsg_SetWindowFocus,
                      bool /* has_focus */)

  IPC_MESSAGE_ROUTED1(PluginMsg_SetContainerVisibility,
                      bool /* is_visible */)
#endif

  IPC_SYNC_MESSAGE_ROUTED2_0(PluginMsg_WillSendRequest,
                             unsigned long /* id */,
                             GURL /* url */)

  IPC_MESSAGE_ROUTED1(PluginMsg_DidReceiveResponse,
                      PluginMsg_DidReceiveResponseParams)

  IPC_MESSAGE_ROUTED3(PluginMsg_DidReceiveData,
                      unsigned long /* id */,
                      std::vector<char> /* buffer */,
                      int /* data_offset */)

  IPC_MESSAGE_ROUTED1(PluginMsg_DidFinishLoading,
                      unsigned long /* id */)

  IPC_MESSAGE_ROUTED1(PluginMsg_DidFail,
                      unsigned long /* id */)

  IPC_MESSAGE_ROUTED4(PluginMsg_SendJavaScriptStream,
                      GURL /* url */,
                      std::string /* result */,
                      bool /* success */,
                      int /* notify_id */)

  IPC_MESSAGE_ROUTED2(PluginMsg_DidReceiveManualResponse,
                      GURL /* url */,
                      PluginMsg_DidReceiveResponseParams)

  IPC_MESSAGE_ROUTED1(PluginMsg_DidReceiveManualData,
                      std::vector<char> /* buffer */)

  IPC_MESSAGE_ROUTED0(PluginMsg_DidFinishManualLoading)

  IPC_MESSAGE_ROUTED0(PluginMsg_DidManualLoadFail)

  IPC_MESSAGE_ROUTED0(PluginMsg_InstallMissingPlugin)

  IPC_MESSAGE_ROUTED3(PluginMsg_HandleURLRequestReply,
                      unsigned long /* resource_id */,
                      GURL /* url */,
                      int /* notify_id */)

  IPC_MESSAGE_ROUTED2(PluginMsg_HTTPRangeRequestReply,
                      unsigned long /* resource_id */,
                      int /* range_request_id */)

  IPC_SYNC_MESSAGE_ROUTED0_1(PluginMsg_CreateCommandBuffer,
                             int /* route_id */)

  IPC_MESSAGE_CONTROL1(PluginMsg_SignalModalDialogEvent,
                       gfx::NativeViewId /* containing_window */)

  IPC_MESSAGE_CONTROL1(PluginMsg_ResetModalDialogEvent,
                       gfx::NativeViewId /* containing_window */)

#if defined(OS_MACOSX)
  // This message, used only on 10.6 and later, transmits the "fake"
  // window handle allocated by the browser on behalf of the renderer
  // to the GPU plugin.
  IPC_MESSAGE_ROUTED1(PluginMsg_SetFakeGPUPluginWindowHandle,
                      gfx::PluginWindowHandle /* window */)
#endif

IPC_END_MESSAGES(Plugin)


//-----------------------------------------------------------------------------
// PluginHost messages
// These are messages sent from the plugin process to the renderer process.
// They all map to the corresponding WebPlugin methods.
IPC_BEGIN_MESSAGES(PluginHost)
  // Sends the plugin window information to the renderer.
  // The window parameter is a handle to the window if the plugin is a windowed
  // plugin. It is NULL for windowless plugins.
  IPC_SYNC_MESSAGE_ROUTED1_0(PluginHostMsg_SetWindow,
                             gfx::PluginWindowHandle /* window */)

#if defined(OS_WIN)
  // The modal_loop_pump_messages_event parameter is an event handle which is
  // passed in for windowless plugins and is used to indicate if messages
  // are to be pumped in sync calls to the plugin process. Currently used
  // in HandleEvent calls.
  IPC_SYNC_MESSAGE_ROUTED1_0(PluginHostMsg_SetWindowlessPumpEvent,
                             HANDLE /* modal_loop_pump_messages_event */)
#endif

  IPC_MESSAGE_ROUTED1(PluginHostMsg_URLRequest,
                      PluginHostMsg_URLRequest_Params)

  IPC_MESSAGE_ROUTED1(PluginHostMsg_CancelResource,
                      int /* id */)

  IPC_MESSAGE_ROUTED1(PluginHostMsg_InvalidateRect,
                      gfx::Rect /* rect */)

  IPC_SYNC_MESSAGE_ROUTED1_1(PluginHostMsg_GetWindowScriptNPObject,
                             int /* route id */,
                             bool /* success */)

  IPC_SYNC_MESSAGE_ROUTED1_1(PluginHostMsg_GetPluginElement,
                             int /* route id */,
                             bool /* success */)

  IPC_MESSAGE_ROUTED3(PluginHostMsg_SetCookie,
                      GURL /* url */,
                      GURL /* first_party_for_cookies */,
                      std::string /* cookie */)

  IPC_SYNC_MESSAGE_ROUTED2_1(PluginHostMsg_GetCookies,
                             GURL /* url */,
                             GURL /* first_party_for_cookies */,
                             std::string /* cookies */)

  // Asks the browser to show a modal HTML dialog.  The dialog is passed the
  // given arguments as a JSON string, and returns its result as a JSON string
  // through json_retval.
  IPC_SYNC_MESSAGE_ROUTED4_1(PluginHostMsg_ShowModalHTMLDialog,
                              GURL /* url */,
                              int /* width */,
                              int /* height */,
                              std::string /* json_arguments */,
                              std::string /* json_retval */)

  IPC_SYNC_MESSAGE_ROUTED2_2(PluginHostMsg_GetDragData,
                             NPVariant_Param /* event */,
                             bool /* add_data */,
                             std::vector<NPVariant_Param> /* result_values */,
                             bool /* result_success */)

  IPC_SYNC_MESSAGE_ROUTED2_1(PluginHostMsg_SetDropEffect,
                             NPVariant_Param /* event */,
                             int /* effect */,
                             bool /* result_success */)

  IPC_MESSAGE_ROUTED1(PluginHostMsg_MissingPluginStatus,
                      int /* status */)

  IPC_SYNC_MESSAGE_ROUTED0_1(PluginHostMsg_GetCPBrowsingContext,
                             uint32 /* context */)

  IPC_MESSAGE_ROUTED0(PluginHostMsg_CancelDocumentLoad)

  IPC_MESSAGE_ROUTED3(PluginHostMsg_InitiateHTTPRangeRequest,
                      std::string /* url */,
                      std::string /* range_info */,
                      int         /* range_request_id */)

  IPC_MESSAGE_ROUTED2(PluginHostMsg_DeferResourceLoading,
                      unsigned long /* resource_id */,
                      bool /* defer */)

  IPC_SYNC_MESSAGE_CONTROL1_0(PluginHostMsg_SetException,
                              std::string /* message */)

#if defined(OS_MACOSX)
  IPC_MESSAGE_ROUTED1(PluginHostMsg_UpdateGeometry_ACK,
                      int /* ack_key */)

  // This message, used only on 10.6 and later, is sent from the
  // plug-in process to the renderer process to indicate that the GPU
  // plugin allocated a new IOSurface object of the given width and
  // height. This information is then forwarded on to the browser
  // process.
  //
  // NOTE: the original intent was to pass a mach port as the
  // IOSurface identifier but it looks like that will be a lot of
  // work. For now we pass an ID from IOSurfaceGetID.
  IPC_MESSAGE_ROUTED4(PluginHostMsg_GPUPluginSetIOSurface,
                      gfx::PluginWindowHandle /* window */,
                      int32 /* width */,
                      int32 /* height */,
                      uint64 /* identifier for IOSurface */)

  // This message, currently used only on 10.6 and later, notifies the
  // renderer process (and from there the browser process) that the
  // GPU plugin swapped the buffers associated with the given
  // "window", which should cause the browser to redraw the various
  // GPU plugins' contents.
  IPC_MESSAGE_ROUTED1(PluginHostMsg_GPUPluginBuffersSwapped,
                      gfx::PluginWindowHandle /* window */)
#endif

IPC_END_MESSAGES(PluginHost)

//-----------------------------------------------------------------------------
// NPObject messages
// These are messages used to marshall NPObjects.  They are sent both from the
// plugin to the renderer and from the renderer to the plugin.
IPC_BEGIN_MESSAGES(NPObject)
  IPC_SYNC_MESSAGE_ROUTED0_0(NPObjectMsg_Release)

  IPC_SYNC_MESSAGE_ROUTED1_1(NPObjectMsg_HasMethod,
                             NPIdentifier_Param /* name */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED3_2(NPObjectMsg_Invoke,
                             bool /* is_default */,
                             NPIdentifier_Param /* method */,
                             std::vector<NPVariant_Param> /* args */,
                             NPVariant_Param /* result_param */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED1_1(NPObjectMsg_HasProperty,
                             NPIdentifier_Param /* name */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED1_2(NPObjectMsg_GetProperty,
                             NPIdentifier_Param /* name */,
                             NPVariant_Param /* property */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED2_1(NPObjectMsg_SetProperty,
                             NPIdentifier_Param /* name */,
                             NPVariant_Param /* property */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED1_1(NPObjectMsg_RemoveProperty,
                             NPIdentifier_Param /* name */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED0_0(NPObjectMsg_Invalidate)

  IPC_SYNC_MESSAGE_ROUTED0_2(NPObjectMsg_Enumeration,
                             std::vector<NPIdentifier_Param> /* value */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED1_2(NPObjectMsg_Construct,
                             std::vector<NPVariant_Param> /* args */,
                             NPVariant_Param /* result_param */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED2_2(NPObjectMsg_Evaluate,
                             std::string /* script */,
                             bool /* popups_allowed */,
                             NPVariant_Param /* result_param */,
                             bool /* result */)

IPC_END_MESSAGES(NPObject)
