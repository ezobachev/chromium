// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "chrome/browser/autocomplete_history_manager.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/automation/automation_tab_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/register_protocol_handler_infobar_delegate.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/extensions/extension_webnavigation_api.h"
#include "chrome/browser/external_protocol/external_protocol_observer.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/omnibox_search_hint.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager_delegate_impl.h"
#include "chrome/browser/pdf_unsupported_feature.h"
#include "chrome/browser/plugin_observer.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_observer.h"
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/safe_browsing/client_side_detection_host.h"
#include "chrome/browser/tab_contents/infobar.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/insecure_content_infobar_delegate.h"
#include "chrome/browser/tab_contents/simple_alert_infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents_ssl_helper.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper_delegate.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/custom_handlers/protocol_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/navigation_details.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/common/notification_service.h"
#include "content/common/view_messages.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/platform_locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/webpreferences.h"

namespace {

static base::LazyInstance<PropertyAccessor<TabContentsWrapper*> >
    g_tab_contents_wrapper_property_accessor(base::LINKER_INITIALIZED);

// The list of prefs we want to observe.
const char* kPrefsToObserve[] = {
  prefs::kAlternateErrorPagesEnabled,
  prefs::kDefaultCharset,
  prefs::kDefaultZoomLevel,
  prefs::kEnableReferrers,
#if defined (ENABLE_SAFE_BROWSING)
  prefs::kSafeBrowsingEnabled,
#endif
  prefs::kWebKitAllowDisplayingInsecureContent,
  prefs::kWebKitAllowRunningInsecureContent,
  prefs::kWebKitDefaultFixedFontSize,
  prefs::kWebKitDefaultFontSize,
  prefs::kWebKitFixedFontFamily,
  prefs::kWebKitJavaEnabled,
  prefs::kWebKitJavascriptEnabled,
  prefs::kWebKitLoadsImagesAutomatically,
  prefs::kWebKitMinimumFontSize,
  prefs::kWebKitMinimumLogicalFontSize,
  prefs::kWebKitPluginsEnabled,
  prefs::kWebKitSansSerifFontFamily,
  prefs::kWebKitSerifFontFamily,
  prefs::kWebKitStandardFontFamily,
  prefs::kWebkitTabsToLinks,
  prefs::kWebKitUsesUniversalDetector
};

const int kPrefsToObserveLength = arraysize(kPrefsToObserve);

}

////////////////////////////////////////////////////////////////////////////////
// TabContentsWrapper, public:

TabContentsWrapper::TabContentsWrapper(TabContents* contents)
    : TabContentsObserver(contents),
      delegate_(NULL),
      infobars_enabled_(true),
      in_destructor_(false),
      tab_contents_(contents) {
  DCHECK(contents);
  DCHECK(!GetCurrentWrapperForContents(contents));
  // Stash this in the property bag so it can be retrieved without having to
  // go to a Browser.
  property_accessor()->SetProperty(contents->property_bag(), this);

  // Create the tab helpers.
  autocomplete_history_manager_.reset(new AutocompleteHistoryManager(contents));
  autofill_manager_.reset(new AutofillManager(this));
  automation_tab_helper_.reset(new AutomationTabHelper(contents));
  blocked_content_tab_helper_.reset(new BlockedContentTabHelper(this));
  bookmark_tab_helper_.reset(new BookmarkTabHelper(this));
  extension_tab_helper_.reset(new ExtensionTabHelper(this));
  favicon_tab_helper_.reset(new FaviconTabHelper(contents));
  find_tab_helper_.reset(new FindTabHelper(contents));
  history_tab_helper_.reset(new HistoryTabHelper(contents));
  restore_tab_helper_.reset(new RestoreTabHelper(this));
  password_manager_delegate_.reset(new PasswordManagerDelegateImpl(this));
  password_manager_.reset(
      new PasswordManager(contents, password_manager_delegate_.get()));
#if defined(ENABLE_SAFE_BROWSING)
  if (profile()->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled) &&
      g_browser_process->safe_browsing_detection_service()) {
    safebrowsing_detection_host_.reset(
        safe_browsing::ClientSideDetectionHost::Create(contents));
  }
#endif
  search_engine_tab_helper_.reset(new SearchEngineTabHelper(contents));
  ssl_helper_.reset(new TabContentsSSLHelper(this));
  content_settings_.reset(new TabSpecificContentSettings(contents));
  translate_tab_helper_.reset(new TranslateTabHelper(contents));
  print_view_manager_.reset(new printing::PrintViewManager(this));

  // Create the per-tab observers.
  external_protocol_observer_.reset(new ExternalProtocolObserver(contents));
  file_select_observer_.reset(new FileSelectObserver(contents));
  plugin_observer_.reset(new PluginObserver(this));
  prerender_observer_.reset(new prerender::PrerenderObserver(this));
  print_preview_.reset(new printing::PrintPreviewMessageHandler(contents));
  webnavigation_observer_.reset(
      new ExtensionWebNavigationTabObserver(contents));
  // Start the in-browser thumbnailing if the feature is enabled.
  if (switches::IsInBrowserThumbnailingEnabled()) {
    thumbnail_generation_observer_.reset(new ThumbnailGenerator);
    thumbnail_generation_observer_->StartThumbnailing(tab_contents_.get());
  }

  // Set-up the showing of the omnibox search infobar if applicable.
  if (OmniboxSearchHint::IsEnabled(contents->profile()))
    omnibox_search_hint_.reset(new OmniboxSearchHint(this));

  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
                 NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_USER_STYLE_SHEET_UPDATED,
                 NotificationService::AllSources());
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
#endif

  // Register for notifications about all interested prefs change.
  PrefService* prefs = profile()->GetPrefs();
  pref_change_registrar_.Init(prefs);
  if (prefs) {
    for (int i = 0; i < kPrefsToObserveLength; ++i)
      pref_change_registrar_.Add(kPrefsToObserve[i], this);
  }

  renderer_preferences_util::UpdateFromSystemSettings(
      tab_contents()->GetMutableRendererPrefs(), profile());
}

TabContentsWrapper::~TabContentsWrapper() {
  in_destructor_ = true;

  // Destroy all remaining InfoBars.  It's important to not animate here so that
  // we guarantee that we'll delete all delegates before we do anything else.
  //
  // TODO(pkasting): If there is no InfoBarContainer, this leaks all the
  // InfoBarDelegates.  This will be fixed once we call CloseSoon() directly on
  // Infobars.
  RemoveAllInfoBars(false);
}

PropertyAccessor<TabContentsWrapper*>* TabContentsWrapper::property_accessor() {
  return g_tab_contents_wrapper_property_accessor.Pointer();
}

void TabContentsWrapper::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kAlternateErrorPagesEnabled,
                             true,
                             PrefService::SYNCABLE_PREF);

  WebPreferences pref_defaults;
  prefs->RegisterBooleanPref(prefs::kWebKitJavascriptEnabled,
                             pref_defaults.javascript_enabled,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitWebSecurityEnabled,
                             pref_defaults.web_security_enabled,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(
      prefs::kWebKitJavascriptCanOpenWindowsAutomatically,
      true,
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitLoadsImagesAutomatically,
                             pref_defaults.loads_images_automatically,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitPluginsEnabled,
                             pref_defaults.plugins_enabled,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitDomPasteEnabled,
                             pref_defaults.dom_paste_enabled,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitShrinksStandaloneImagesToFit,
                             pref_defaults.shrinks_standalone_images_to_fit,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kWebKitInspectorSettings,
                                PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitTextAreasAreResizable,
                             pref_defaults.text_areas_are_resizable,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitJavaEnabled,
                             pref_defaults.java_enabled,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebkitTabsToLinks,
                             pref_defaults.tabs_to_links,
                             PrefService::UNSYNCABLE_PREF);

#if !defined(OS_MACOSX)
  prefs->RegisterLocalizedStringPref(prefs::kAcceptLanguages,
                                     IDS_ACCEPT_LANGUAGES,
                                     PrefService::SYNCABLE_PREF);
#else
  // Not used in OSX.
  prefs->RegisterLocalizedStringPref(prefs::kAcceptLanguages,
                                     IDS_ACCEPT_LANGUAGES,
                                     PrefService::UNSYNCABLE_PREF);
#endif
  prefs->RegisterLocalizedStringPref(prefs::kDefaultCharset,
                                     IDS_DEFAULT_ENCODING,
                                     PrefService::SYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitStandardFontFamily,
                                     IDS_STANDARD_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitFixedFontFamily,
                                     IDS_FIXED_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitSerifFontFamily,
                                     IDS_SERIF_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitSansSerifFontFamily,
                                     IDS_SANS_SERIF_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitCursiveFontFamily,
                                     IDS_CURSIVE_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitFantasyFontFamily,
                                     IDS_FANTASY_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitDefaultFontSize,
                                      IDS_DEFAULT_FONT_SIZE,
                                      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitDefaultFixedFontSize,
                                      IDS_DEFAULT_FIXED_FONT_SIZE,
                                      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitMinimumFontSize,
                                      IDS_MINIMUM_FONT_SIZE,
                                      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitMinimumLogicalFontSize,
                                      IDS_MINIMUM_LOGICAL_FONT_SIZE,
                                      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedBooleanPref(prefs::kWebKitUsesUniversalDetector,
                                      IDS_USES_UNIVERSAL_DETECTOR,
                                      PrefService::SYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kStaticEncodings,
                                     IDS_STATIC_ENCODING_LIST,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kRecentlySelectedEncoding,
                            "",
                            PrefService::UNSYNCABLE_PREF);
}

string16 TabContentsWrapper::GetDefaultTitle() {
  return l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE);
}

string16 TabContentsWrapper::GetStatusText() const {
  if (!tab_contents()->IsLoading() ||
      tab_contents()->load_state() == net::LOAD_STATE_IDLE) {
    return string16();
  }

  switch (tab_contents()->load_state()) {
    case net::LOAD_STATE_WAITING_FOR_CACHE:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_WAITING_FOR_CACHE);
    case net::LOAD_STATE_ESTABLISHING_PROXY_TUNNEL:
      return
          l10n_util::GetStringUTF16(IDS_LOAD_STATE_ESTABLISHING_PROXY_TUNNEL);
    case net::LOAD_STATE_RESOLVING_PROXY_FOR_URL:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_RESOLVING_PROXY_FOR_URL);
    case net::LOAD_STATE_RESOLVING_HOST:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_RESOLVING_HOST);
    case net::LOAD_STATE_CONNECTING:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_CONNECTING);
    case net::LOAD_STATE_SSL_HANDSHAKE:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_SSL_HANDSHAKE);
    case net::LOAD_STATE_SENDING_REQUEST:
      if (tab_contents()->upload_size())
        return l10n_util::GetStringFUTF16Int(
                    IDS_LOAD_STATE_SENDING_REQUEST_WITH_PROGRESS,
                    static_cast<int>((100 * tab_contents()->upload_position()) /
                        tab_contents()->upload_size()));
      else
        return l10n_util::GetStringUTF16(IDS_LOAD_STATE_SENDING_REQUEST);
    case net::LOAD_STATE_WAITING_FOR_RESPONSE:
      return l10n_util::GetStringFUTF16(IDS_LOAD_STATE_WAITING_FOR_RESPONSE,
                                        tab_contents()->load_state_host());
    // Ignore net::LOAD_STATE_READING_RESPONSE and net::LOAD_STATE_IDLE
    case net::LOAD_STATE_IDLE:
    case net::LOAD_STATE_READING_RESPONSE:
      break;
  }

  return string16();
}

TabContentsWrapper* TabContentsWrapper::Clone() {
  TabContents* new_contents = tab_contents()->Clone();
  TabContentsWrapper* new_wrapper = new TabContentsWrapper(new_contents);

  new_wrapper->extension_tab_helper()->CopyStateFrom(
      *extension_tab_helper_.get());
  return new_wrapper;
}

void TabContentsWrapper::CaptureSnapshot() {
  Send(new ViewMsg_CaptureSnapshot(routing_id()));
}

// static
TabContentsWrapper* TabContentsWrapper::GetCurrentWrapperForContents(
    TabContents* contents) {
  TabContentsWrapper** wrapper =
      property_accessor()->GetProperty(contents->property_bag());

  return wrapper ? *wrapper : NULL;
}

// static
const TabContentsWrapper* TabContentsWrapper::GetCurrentWrapperForContents(
    const TabContents* contents) {
  TabContentsWrapper* const* wrapper =
      property_accessor()->GetProperty(contents->property_bag());

  return wrapper ? *wrapper : NULL;
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsWrapper implementation:

void TabContentsWrapper::RenderViewCreated(RenderViewHost* render_view_host) {
  UpdateAlternateErrorPageURL(render_view_host);
}

void TabContentsWrapper::RenderViewGone() {
  RemoveAllInfoBars(true);

  // Tell the view that we've crashed so it can prepare the sad tab page.
  // Only do this if we're not in browser shutdown, so that TabContents
  // objects that are not in a browser (e.g., HTML dialogs) and thus are
  // visible do not flash a sad tab page.
  if (browser_shutdown::GetShutdownType() == browser_shutdown::NOT_VALID) {
    tab_contents()->view()->OnTabCrashed(
        tab_contents()->crashed_status(), tab_contents()->crashed_error_code());
  }
}

void TabContentsWrapper::DidBecomeSelected() {
  WebCacheManager::GetInstance()->ObserveActivity(
      tab_contents()->GetRenderProcessHost()->id());
}

bool TabContentsWrapper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TabContentsWrapper, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_JSOutOfMemory, OnJSOutOfMemory)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RegisterProtocolHandler,
                        OnRegisterProtocolHandler)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Snapshot, OnSnapshot)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PDFHasUnsupportedFeature,
                        OnPDFHasUnsupportedFeature)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidBlockDisplayingInsecureContent,
                        OnDidBlockDisplayingInsecureContent)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidBlockRunningInsecureContent,
                        OnDidBlockRunningInsecureContent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void TabContentsWrapper::TabContentsDestroyed(TabContents* tab) {
  // Destruction of the TabContents should only be done by us from our
  // destructor. Otherwise it's very likely we (or one of the helpers we own)
  // will attempt to access the TabContents and we'll crash.
  DCHECK(in_destructor_);
}

void TabContentsWrapper::Observe(int type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
      DCHECK(&tab_contents_->controller() ==
             Source<NavigationController>(source).ptr());

      content::LoadCommittedDetails& committed_details =
          *(Details<content::LoadCommittedDetails>(details).ptr());

      // NOTE: It is not safe to change the following code to count upwards or
      // use iterators, as the RemoveInfoBar() call synchronously modifies our
      // delegate list.
      for (size_t i = infobars_.size(); i > 0; --i) {
        InfoBarDelegate* delegate = GetInfoBarDelegateAt(i - 1);
        if (delegate->ShouldExpire(committed_details))
          RemoveInfoBar(delegate);
      }

      break;
    }
    case chrome::NOTIFICATION_GOOGLE_URL_UPDATED:
      UpdateAlternateErrorPageURL(render_view_host());
      break;
    case chrome::NOTIFICATION_USER_STYLE_SHEET_UPDATED:
      UpdateWebPreferences();
      break;
#if defined(OS_POSIX) && !defined(OS_MACOSX)
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED: {
      UpdateRendererPreferences();
      break;
    }
#endif
    case chrome::NOTIFICATION_PREF_CHANGED: {
      std::string* pref_name_in = Details<std::string>(details).ptr();
      DCHECK(Source<PrefService>(source).ptr() == profile()->GetPrefs());
      if (*pref_name_in == prefs::kAlternateErrorPagesEnabled) {
        UpdateAlternateErrorPageURL(render_view_host());
      } else if ((*pref_name_in == prefs::kDefaultCharset) ||
                 StartsWithASCII(*pref_name_in, "webkit.webprefs.", true)) {
        UpdateWebPreferences();
      } else if (*pref_name_in == prefs::kDefaultZoomLevel) {
        Send(new ViewMsg_SetZoomLevel(
            routing_id(), tab_contents()->GetZoomLevel()));
      } else if (*pref_name_in == prefs::kEnableReferrers) {
        UpdateRendererPreferences();
      } else if (*pref_name_in == prefs::kSafeBrowsingEnabled) {
        UpdateSafebrowsingDetectionHost();
      } else {
        NOTREACHED() << "unexpected pref change notification" << *pref_name_in;
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void TabContentsWrapper::AddInfoBar(InfoBarDelegate* delegate) {
  if (!infobars_enabled_) {
    delegate->InfoBarClosed();
    return;
  }

  for (size_t i = 0; i < infobars_.size(); ++i) {
    if (GetInfoBarDelegateAt(i)->EqualsDelegate(delegate)) {
      delegate->InfoBarClosed();
      return;
    }
  }

  infobars_.push_back(delegate);
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
      Source<TabContentsWrapper>(this), Details<InfoBarAddedDetails>(delegate));

  // Add ourselves as an observer for navigations the first time a delegate is
  // added. We use this notification to expire InfoBars that need to expire on
  // page transitions.
  if (infobars_.size() == 1) {
    registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                   Source<NavigationController>(&tab_contents_->controller()));
  }
}

void TabContentsWrapper::RemoveInfoBar(InfoBarDelegate* delegate) {
  RemoveInfoBarInternal(delegate, true);
}

void TabContentsWrapper::ReplaceInfoBar(InfoBarDelegate* old_delegate,
                                        InfoBarDelegate* new_delegate) {
  if (!infobars_enabled_) {
    AddInfoBar(new_delegate);  // Deletes the delegate.
    return;
  }

  size_t i;
  for (i = 0; i < infobars_.size(); ++i) {
    if (GetInfoBarDelegateAt(i) == old_delegate)
      break;
  }
  DCHECK_LT(i, infobars_.size());

  infobars_.insert(infobars_.begin() + i, new_delegate);

  InfoBarReplacedDetails replaced_details(old_delegate, new_delegate);
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REPLACED,
      Source<TabContentsWrapper>(this),
      Details<InfoBarReplacedDetails>(&replaced_details));

  infobars_.erase(infobars_.begin() + i + 1);
}

InfoBarDelegate* TabContentsWrapper::GetInfoBarDelegateAt(size_t index) {
  return infobars_[index];
}

////////////////////////////////////////////////////////////////////////////////
// Internal helpers

void TabContentsWrapper::OnJSOutOfMemory() {
  AddInfoBar(new SimpleAlertInfoBarDelegate(tab_contents(),
      NULL, l10n_util::GetStringUTF16(IDS_JS_OUT_OF_MEMORY_PROMPT), true));
}

void TabContentsWrapper::OnRegisterProtocolHandler(const std::string& protocol,
                                                   const GURL& url,
                                                   const string16& title) {
  if (profile()->IsOffTheRecord())
    return;

  ChildProcessSecurityPolicy* policy =
      ChildProcessSecurityPolicy::GetInstance();
  if (policy->IsPseudoScheme(protocol) || policy->IsDisabledScheme(protocol))
    return;

  ProtocolHandler handler =
      ProtocolHandler::CreateProtocolHandler(protocol, url, title);

  ProtocolHandlerRegistry* registry = profile()->GetProtocolHandlerRegistry();
  if (!registry->enabled() || registry->IsRegistered(handler))
    return;

  if (!handler.IsEmpty() &&
      registry->CanSchemeBeOverridden(handler.protocol())) {
    AddInfoBar(new RegisterProtocolHandlerInfoBarDelegate(tab_contents(),
                                                          registry,
                                                          handler));
  }
}

void TabContentsWrapper::OnSnapshot(const SkBitmap& bitmap) {
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_SNAPSHOT_TAKEN,
      Source<TabContentsWrapper>(this),
      Details<const SkBitmap>(&bitmap));
}

void TabContentsWrapper::OnPDFHasUnsupportedFeature() {
  PDFHasUnsupportedFeature(this);
}

void TabContentsWrapper::OnDidBlockDisplayingInsecureContent() {
  // At most one infobar and do not supersede the stronger running content bar.
  for (size_t i = 0; i < infobars_.size(); ++i) {
    if (GetInfoBarDelegateAt(i)->AsInsecureContentInfoBarDelegate())
      return;
  }
  AddInfoBar(new InsecureContentInfoBarDelegate(this,
      InsecureContentInfoBarDelegate::DISPLAY));
}

void TabContentsWrapper::OnDidBlockRunningInsecureContent() {
  // At most one infobar superseding any weaker displaying content bar.
  for (size_t i = 0; i < infobars_.size(); ++i) {
    InsecureContentInfoBarDelegate* delegate =
        GetInfoBarDelegateAt(i)->AsInsecureContentInfoBarDelegate();
    if (delegate) {
      if (delegate->type() != InsecureContentInfoBarDelegate::RUN) {
        ReplaceInfoBar(delegate, new InsecureContentInfoBarDelegate(this,
            InsecureContentInfoBarDelegate::RUN));
      }
      return;
    }
  }
  AddInfoBar(new InsecureContentInfoBarDelegate(this,
      InsecureContentInfoBarDelegate::RUN));
}

GURL TabContentsWrapper::GetAlternateErrorPageURL() const {
  GURL url;
  // Disable alternate error pages when in Incognito mode.
  if (profile()->IsOffTheRecord())
    return url;

  PrefService* prefs = profile()->GetPrefs();
  if (prefs->GetBoolean(prefs::kAlternateErrorPagesEnabled)) {
    url = google_util::AppendGoogleLocaleParam(
        GURL(google_util::kLinkDoctorBaseURL));
    url = google_util::AppendGoogleTLDParam(url);
  }
  return url;
}

void TabContentsWrapper::UpdateAlternateErrorPageURL(RenderViewHost* rvh) {
  rvh->Send(new ViewMsg_SetAltErrorPageURL(
      rvh->routing_id(), GetAlternateErrorPageURL()));
}

void TabContentsWrapper::UpdateWebPreferences() {
  RenderViewHostDelegate* rvhd = tab_contents();
  Send(new ViewMsg_UpdateWebPreferences(routing_id(), rvhd->GetWebkitPrefs()));
}

void TabContentsWrapper::UpdateRendererPreferences() {
  renderer_preferences_util::UpdateFromSystemSettings(
      tab_contents()->GetMutableRendererPrefs(), profile());
  render_view_host()->SyncRendererPrefs();
}

void TabContentsWrapper::UpdateSafebrowsingDetectionHost() {
#if defined(ENABLE_SAFE_BROWSING)
  PrefService* prefs = profile()->GetPrefs();
  bool safe_browsing = prefs->GetBoolean(prefs::kSafeBrowsingEnabled);
  if (safe_browsing &&
      g_browser_process->safe_browsing_detection_service()) {
    if (!safebrowsing_detection_host_.get()) {
      safebrowsing_detection_host_.reset(
          safe_browsing::ClientSideDetectionHost::Create(tab_contents()));
    }
  } else {
    safebrowsing_detection_host_.reset();
  }
  render_view_host()->Send(
      new ViewMsg_SetClientSidePhishingDetection(routing_id(), safe_browsing));
#endif
}

void TabContentsWrapper::RemoveInfoBarInternal(InfoBarDelegate* delegate,
                                               bool animate) {
  if (!infobars_enabled_) {
    DCHECK(infobars_.empty());
    return;
  }

  size_t i;
  for (i = 0; i < infobars_.size(); ++i) {
    if (GetInfoBarDelegateAt(i) == delegate)
      break;
  }
  DCHECK_LT(i, infobars_.size());
  InfoBarDelegate* infobar = infobars_[i];

  InfoBarRemovedDetails removed_details(infobar, animate);
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
      Source<TabContentsWrapper>(this),
      Details<InfoBarRemovedDetails>(&removed_details));

  infobars_.erase(infobars_.begin() + i);
  // Remove ourselves as an observer if we are tracking no more InfoBars.
  if (infobars_.empty()) {
    registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        Source<NavigationController>(&tab_contents_->controller()));
  }
}

void TabContentsWrapper::RemoveAllInfoBars(bool animate) {
  while (!infobars_.empty())
    RemoveInfoBarInternal(GetInfoBarDelegateAt(infobar_count() - 1), animate);
}
