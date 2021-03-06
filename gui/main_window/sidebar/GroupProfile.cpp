#include "stdafx.h"
#include "GroupProfile.h"
#include "SidebarUtils.h"
#include "GalleryList.h"
#include "ImageVideoList.h"
#include "LinkList.h"
#include "FilesList.h"
#include "PttList.h"
#include "EditNicknameWidget.h"
#include "../MainWindow.h"
#include "../GroupChatOperations.h"
#include "../MainPage.h"
#include "../ReportWidget.h"
#include "../friendly/FriendlyContainer.h"
#include "../contact_list/TopPanel.h"
#include "../contact_list/ChatMembersModel.h"
#include "../contact_list/IgnoreMembersModel.h"
#include "../contact_list/RecentsModel.h"
#include "../contact_list/ContactListModel.h"
#include "../contact_list/SelectionContactsForGroupChat.h"
#include "../contact_list/ContactListUtils.h"
#include "../contact_list/ContactListWidget.h"
#include "../contact_list/ContactListItemDelegate.h"
#include "../contact_list/SearchWidget.h"
#include "../contact_list/ChatContactsModel.h"
#include "../settings/themes/WallpaperDialog.h"
#include "../../previewer/toast.h"
#include "../../styles/ThemeParameters.h"
#include "../../controls/TransparentScrollBar.h"
#include "../../controls/DialogButton.h"
#include "../../controls/ContextMenu.h"
#include "../../controls/GeneralDialog.h"
#include "../../fonts.h"
#include "../../utils/gui_coll_helper.h"
#include "../../utils/stat_utils.h"
#include "../../utils/InterConnector.h"
#include "../../utils/features.h"
#include "../../../common.shared/config/config.h"

namespace
{
    enum widgets
    {
        main = 0,
        settings = 1,
        list = 2,
        gallery = 3,
    };

    enum gallery_page
    {
        photo_and_video = 0,
        video = 1,
        files = 2,
        links = 3,
        ptt = 4,
    };

    enum list_page
    {
        all = 0,
        pending = 1,
        blocked = 2,
        admins = 3,
    };

    enum main_action
    {
        invite = 0,
        unblock = 1,
        join = 2,
    };

    constexpr auto ICON_SIZE = 20;
    constexpr auto BTN_ICON_SIZE = 32;
    constexpr auto AVATAR_SIZE = 52;
    constexpr auto SPACER_MARGIN = 8;
    constexpr auto SHORT_MEMBERS_COUNT = 5;
    constexpr auto APPROVE_ALL_BUTTON_HEIGHT = 48;
    constexpr auto TOAST_OFFSET = 10;
    constexpr auto INFO_SPACER_HEIGHT = 8;
    constexpr auto BOTTOM_SPACER = 40;
    constexpr auto POPUP_HOR_OFFSET = 8;
    constexpr auto POPUP_VER_OFFSET = 52;
    constexpr auto ABOUT_LINK_SPACER_HEIGHT = 24;
    constexpr auto ABOUT_LINK_RIGHT_OFFSET = 16;
    constexpr auto APPLY_SETTINGS_MARGIN = 24;
    constexpr auto SETTINGS_LEFT_OFFSET = 20;
    constexpr auto SETTINGS_ADD_OFFSET = 4;

    bool isYouAdmin(std::shared_ptr<Data::ChatInfo> _info)
    {
        if (!_info)
            return false;

        return _info->YourRole_ == ql1s("admin") || _info->Creator_ == Ui::MyInfo()->aimId();
    }

    bool isYouModer(std::shared_ptr<Data::ChatInfo> _info)
    {
        if (!_info)
            return false;

        return _info->YourRole_ == ql1s("moder");
    }

    bool isYouAdminOrModer(std::shared_ptr<Data::ChatInfo> _info)
    {
        return isYouAdmin(_info) || isYouModer(_info);
    }

    bool isChatControlled(std::shared_ptr<Data::ChatInfo> _info)
    {
        if (!_info)
            return false;

        return _info->Controlled_;
    }

    bool isChannel(std::shared_ptr<Data::ChatInfo> _info)
    {
        if (!_info)
            return false;

        return _info->DefaultRole_ == qsl("readonly");
    }

    QMap<QString, QVariant> makeData(const QString& command, const QString& aimId)
    {
        QMap<QString, QVariant> result;
        result[qsl("command")] = command;
        result[qsl("aimid")] = aimId;
        return result;
    }

    QMap<QString, QVariant> makeCopyData(const QString& _command)
    {
        QMap<QString, QVariant> result;
        result[qsl("command")] = _command;
        return result;
    }
}

namespace Ui
{
    GroupProfile::GroupProfile(QWidget* parent)
        : SidebarPage(parent)
        , frameCountMode_(FrameCountMode::_1)
        , shortChatModel_(new Logic::ChatMembersModel(this))
        , listChatModel_(new Logic::ChatMembersModel(this))
        , currentGalleryPage_(-1)
        , currentListPage_(-1)
        , mainAction_(-1)
        , galleryIsEmpty_(false)
        , forceMembersRefresh_(false)
        , invalidNick_(false)
    {
        init();
    }

    GroupProfile::~GroupProfile()
    {

    }

    void GroupProfile::initFor(const QString& aimId)
    {
        isActive_ = true;

        const auto newContact = currentAimId_ != aimId;
        currentAimId_ = aimId;

        info_->setAimIdAndSize(aimId, Utils::scale_value(AVATAR_SIZE));
        loadChatInfo();

        if (newContact)
        {
            galleryPreview_->setAimId(aimId);
            shortMembersList_->clearCache();

            shortChatModel_->setAimId(aimId);
            listChatModel_->setAimId(aimId);

            updatePinButton();
            updateMuteState();

            auto st = Logic::getContactListModel()->getGalleryState(currentAimId_);
            dialogGalleryState(currentAimId_, st);
            if (st.isEmpty())
                Ui::GetDispatcher()->requestDialogGalleryState(currentAimId_);

            groupStatus_->hide();
            mainActionButton_->hide();

            changeTab(main);
            area_->ensureVisible(0, 0);
        }

        Ui::GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::profilescr_view, { { "chat_type", Utils::chatTypeByAimId(currentAimId_) } });

        invalidNick_ = false;
    }

    void GroupProfile::setFrameCountMode(FrameCountMode _mode)
    {
        if (frameCountMode_ != _mode)
        {
            frameCountMode_ = _mode;
            updateCloseIcon();
            updateChatControls();
        }
    }

    void GroupProfile::close()
    {
        closeGallery();
        changeTab(main);
        area_->ensureVisible(0, 0);

        isActive_ = false;
    }

    void GroupProfile::toggleMembersList()
    {
        if (stackedWidget_->currentIndex() == list)
        {
            changeTab(main);
            closeClicked();
        }
        else
        {
            allMembersClicked();
        }
    }

    void GroupProfile::resizeEvent(QResizeEvent* _event)
    {
        cl_->setWidthForDelegate(_event->size().width());
        galleryPopup_->setFixedWidth(_event->size().width() - Utils::scale_value(POPUP_HOR_OFFSET * 2));
        aboutLinkToChatBlock_->setFixedWidth(_event->size().width());

        SidebarPage::resizeEvent(_event);
    }

    void GroupProfile::init()
    {
        auto layout = Utils::emptyVLayout(this);

        auto areaContainer = new QWidget(this);
        auto areaContainerLayout = Utils::emptyVLayout(areaContainer);

        titleBar_ = new HeaderTitleBar;
        titleBar_->setStyleSheet(qsl("background-color: %1; border-bottom: %2 solid %3;").arg(Styling::getParameters().getColorHex(Styling::StyleVariable::BASE_GLOBALWHITE),
            QString::number(Utils::scale_value(1)) + ql1s("px"),
            Styling::getParameters().getColorHex(Styling::StyleVariable::BASE_BRIGHT)));
        titleBar_->setTitle(QT_TRANSLATE_NOOP("sidebar", "Information"));
        const auto headerIconSize = QSize(ICON_SIZE, ICON_SIZE);
        titleBar_->setButtonSize(Utils::scale_value(headerIconSize));
        {
            editButton_ = new HeaderTitleBarButton(this);
            editButton_->setDefaultImage(qsl(":/context_menu/edit"), Styling::getParameters().getColor(Styling::StyleVariable::BASE_SECONDARY), headerIconSize);
            titleBar_->addButtonToRight(editButton_);
            connect(editButton_, &HeaderTitleBarButton::clicked, this, &GroupProfile::editButtonClicked);
        }
        {
            closeButton_ = new HeaderTitleBarButton(this);
            titleBar_->addButtonToLeft(closeButton_);
            connect(closeButton_, &HeaderTitleBarButton::clicked, this, &GroupProfile::closeClicked);
        }

        connect(titleBar_, &HeaderTitleBar::arrowClicked, this, &GroupProfile::titleArrowClicked);

        areaContainerLayout->addWidget(titleBar_);
        stackedWidget_ = new QStackedWidget(areaContainer);
        areaContainerLayout->addWidget(stackedWidget_);

        delegate_ = new Logic::ContactListItemDelegate(this, Logic::MEMBERS_LIST);
        delegate_->setMembersView();

        area_ = CreateScrollAreaAndSetTrScrollBarV(stackedWidget_);
        area_->setWidget(initContent(area_));
        area_->setWidgetResizable(true);
        area_->setFrameStyle(QFrame::NoFrame);
        area_->horizontalScrollBar()->setEnabled(false);
        Utils::transparentBackgroundStylesheet(area_);

        stackedWidget_->insertWidget(main, area_);
        stackedWidget_->insertWidget(settings, initSettings(stackedWidget_));
        stackedWidget_->insertWidget(list, initContactList(stackedWidget_));
        stackedWidget_->insertWidget(gallery, initGallery(stackedWidget_));

        stackedWidget_->setCurrentIndex(main);

        layout->addWidget(areaContainer);

        updateCloseIcon();
        setMouseTracking(true);

        connect(Ui::GetDispatcher(), &Ui::core_dispatcher::chatInfo, this, &GroupProfile::chatInfo);
        connect(Ui::GetDispatcher(), &Ui::core_dispatcher::chatInfoFailed, this, &GroupProfile::chatInfoFailed);
        connect(Ui::GetDispatcher(), &Ui::core_dispatcher::dialogGalleryState, this, &GroupProfile::dialogGalleryState);
        connect(Logic::getRecentsModel(), &Logic::RecentsModel::favoriteChanged, this, &GroupProfile::favoriteChanged);
        connect(Logic::getRecentsModel(), &Logic::RecentsModel::unimportantChanged, this, &GroupProfile::unimportantChanged);
        connect(Ui::GetDispatcher(), &Ui::core_dispatcher::setChatRoleResult, this, &GroupProfile::memberActionResult);
        connect(Ui::GetDispatcher(), &Ui::core_dispatcher::blockMemberResult, this, &GroupProfile::memberActionResult);
        connect(Ui::GetDispatcher(), &Ui::core_dispatcher::pendingListResult, this, &GroupProfile::memberActionResult);
        connect(Ui::GetDispatcher(), &Ui::core_dispatcher::modChatParamsResult, this, &GroupProfile::modChatParamsResult);
        connect(Ui::GetDispatcher(), &Ui::core_dispatcher::suggestGroupNickResult, this, &GroupProfile::suggestGroupNickResult);
        connect(Ui::GetDispatcher(), &Ui::core_dispatcher::modChatParamsResult, this, &GroupProfile::loadChatInfo);
        connect(&Utils::InterConnector::instance(), &Utils::InterConnector::chatEvents, this, &GroupProfile::chatEvents);
        connect(Logic::getContactListModel(), &Logic::ContactListModel::contactChanged, this, &GroupProfile::onContactChanged);
        connect(Logic::getRecentsModel(), &Logic::RecentsModel::favoriteChanged, this, &GroupProfile::favoriteChanged);
    }

    QWidget* GroupProfile::initContent(QWidget* _parent)
    {
        auto widget = new QWidget(_parent);
        Utils::transparentBackgroundStylesheet(widget);
        Utils::emptyContentsMargins(widget);

        auto layout = Utils::emptyVLayout(widget);
        layout->setAlignment(Qt::AlignTop);

        {
            info_ = addAvatarInfo(widget, layout);
            connect(info_, &AvatarNameInfo::avatarClicked, this, &GroupProfile::avatarClicked);

            infoSpacer_ = new QWidget(widget);
            Utils::transparentBackgroundStylesheet(infoSpacer_);
            infoSpacer_->setFixedHeight(INFO_SPACER_HEIGHT);
            layout->addWidget(infoSpacer_);

            groupStatus_ = addLabel(QString(), widget, layout);

            mainActionButton_ = addColoredButton(QString(), QString(), widget, layout);
            connect(mainActionButton_, &ColoredButton::clicked, this, &GroupProfile::mainActionClicked);

            about_ = addInfoBlock(QString(), QString(), widget, layout);
            about_->text_->makeCopyable();
            connect(about_->text_, &TextLabel::menuAction, this, &GroupProfile::menuAction);

            rules_ = addInfoBlock(QT_TRANSLATE_NOOP("siderbar", "Rules"), QString(), widget, layout);
            rules_->text_->makeCopyable();
            connect(rules_->text_, &TextLabel::menuAction, this, &GroupProfile::menuAction);

            link_ = addInfoBlock(QT_TRANSLATE_NOOP("siderbar", "Link"), QString(), widget, layout);
            link_->text_->showButtons();
            connect(link_->text_, &TextLabel::textClicked, this, &GroupProfile::linkClicked);
            connect(link_->text_, &TextLabel::copyClicked, this, &GroupProfile::linkCopy);
            connect(link_->text_, &TextLabel::shareClicked, this, &GroupProfile::linkShare);
            connect(link_->text_, &TextLabel::menuAction, this, &GroupProfile::menuAction);
        }

        addSpacer(widget, layout);

        {
            share_ = addButton(qsl(":/share_icon"), QT_TRANSLATE_NOOP("sidebar", "Share group"), widget, layout);
            connect(share_, &SidebarButton::clicked, this, &GroupProfile::shareClicked);

            notifications_ = addCheckbox(qsl(":/notify_icon"), QT_TRANSLATE_NOOP("sidebar", "Notifications"), widget, layout);
            connect(notifications_, &SidebarCheckboxButton::checked, this, &GroupProfile::notificationsChecked);

            settings_ = addButton(qsl(":/settings_icon_sidebar"), QT_TRANSLATE_NOOP("sidebar", "Group settings"), widget, layout);
            connect(settings_, &SidebarButton::clicked, this, &GroupProfile::settingsClicked);
        }

        secondSpacer_ = addSpacer(widget, layout);

        {
            galleryWidget_ = new QWidget(widget);
            auto galleryLayout = Utils::emptyVLayout(galleryWidget_);

            galleryPhoto_ = addButton(qsl(":/background_icon"), QT_TRANSLATE_NOOP("sidebar", "Photo and video"), galleryWidget_, galleryLayout);
            connect(galleryPhoto_, &SidebarButton::clicked, this, &GroupProfile::galleryPhotoCLicked);

            galleryPreview_ = addGalleryPrevieWidget(galleryWidget_, galleryLayout);

            galleryVideo_ = addButton(qsl(":/video_icon"), QT_TRANSLATE_NOOP("sidebar", "Video"), galleryWidget_, galleryLayout);
            connect(galleryVideo_, &SidebarButton::clicked, this, &GroupProfile::galleryVideoCLicked);

            galleryFiles_ = addButton(qsl(":/gallery/file_icon"), QT_TRANSLATE_NOOP("sidebar", "Files"), galleryWidget_, galleryLayout);
            connect(galleryFiles_, &SidebarButton::clicked, this, &GroupProfile::galleryFilesCLicked);

            galleryLinks_ = addButton(qsl(":/copy_link_icon"), QT_TRANSLATE_NOOP("sidebar", "Links"), galleryWidget_, galleryLayout);
            connect(galleryLinks_, &SidebarButton::clicked, this, &GroupProfile::galleryLinksCLicked);

            galleryPtt_ = addButton(qsl(":/gallery/micro_icon"), QT_TRANSLATE_NOOP("sidebar", "Voice messages"), galleryWidget_, galleryLayout);
            connect(galleryPtt_, &SidebarButton::clicked, this, &GroupProfile::galleryPttCLicked);

            layout->addWidget(galleryWidget_);
        }

        thirdSpacer_ = addSpacer(widget, layout);

        {
            membersWidget_ = new QWidget(widget);
            auto membersLayout = Utils::emptyVLayout(membersWidget_);

            members_ = addMembersPlate(membersWidget_, membersLayout);
            connect(members_, &MembersPlate::searchClicked, this, &GroupProfile::searchMembersClicked);

            addToChat_ = addButton(qsl(":/add_groupchat"), QT_TRANSLATE_NOOP("sidebar", "Add to chat"), membersWidget_, membersLayout);
            connect(addToChat_, &SidebarButton::clicked, this, &GroupProfile::addToChatClicked);

            pendings_ = addButton(qsl(":/waiting_for_accept_icon"), QT_TRANSLATE_NOOP("sidebar", "Waiting for approval"), membersWidget_, membersLayout);
            connect(pendings_, &SidebarButton::clicked, this, &GroupProfile::pendingsClicked);
            connect(&Utils::InterConnector::instance(), &Utils::InterConnector::showPendingMembers, this, &GroupProfile::pendingsClicked);

            shortMembersList_ = addMembersWidget(shortChatModel_, delegate_, SHORT_MEMBERS_COUNT, membersWidget_, membersLayout);
            connect(shortMembersList_, &MembersWidget::selected, this, &GroupProfile::contactSelected);
            connect(shortMembersList_, &MembersWidget::removeClicked, this, &GroupProfile::contactRemoved);
            connect(shortMembersList_, &MembersWidget::moreClicked, this, &GroupProfile::contactMenuRequested);

            admins_ = addButton(qsl(":/admins_icon"), QT_TRANSLATE_NOOP("sidebar", "Admins"), membersWidget_, membersLayout);
            connect(admins_, &SidebarButton::clicked, this, &GroupProfile::adminsClicked);

            allMembers_ = addButton(qsl(":/members_icon"), QT_TRANSLATE_NOOP("sidebar", "All members"), membersWidget_, membersLayout);
            connect(allMembers_, &SidebarButton::clicked, this, &GroupProfile::allMembersClicked);

            blockedMembers_ = addButton(qsl(":/blocked_users_icon"), QT_TRANSLATE_NOOP("sidebar", "Blocked people"), membersWidget_, membersLayout);
            connect(blockedMembers_, &SidebarButton::clicked, this, &GroupProfile::blockedMembersClicked);

            layout->addWidget(membersWidget_);
        }

        fourthSpacer_ = addSpacer(widget, layout);

        {
            pin_ = addButton(qsl(":/pin_chat_icon"), QT_TRANSLATE_NOOP("sidebar", "Pin"), widget, layout);
            connect(pin_, &SidebarButton::clicked, this, &GroupProfile::pinClicked);

            theme_ = addButton(qsl(":/colors_icon"), QT_TRANSLATE_NOOP("sidebar", "Wallpaper"), widget, layout);
            connect(theme_, &SidebarButton::clicked, this, &GroupProfile::themeClicked);
        }

        fifthSpacer_ = addSpacer(widget, layout);

        {
            clearHistory_ = addButton(qsl(":/clear_chat_icon"), QT_TRANSLATE_NOOP("sidebar", "Clear history"), widget, layout);
            connect(clearHistory_, &SidebarButton::clicked, this, &GroupProfile::clearHistoryClicked);

            block_ = addButton(qsl(":/ignore_icon"), QT_TRANSLATE_NOOP("sidebar", "Block"), widget, layout);
            connect(block_, &SidebarButton::clicked, this, &GroupProfile::blockClicked);

            report_ = addButton(qsl(":/alert_icon"), QT_TRANSLATE_NOOP("sidebar", "Report and block"), widget, layout);
            connect(report_, &SidebarButton::clicked, this, &GroupProfile::reportCliked);

            leave_ = addButton(qsl(":/exit_icon"), QT_TRANSLATE_NOOP("sidebar", "Leave and delete"), widget, layout);
            connect(leave_, &SidebarButton::clicked, this, &GroupProfile::leaveClicked);
        }

        layout->addSpacerItem(new QSpacerItem(0, Utils::scale_value(BOTTOM_SPACER), QSizePolicy::Preferred, QSizePolicy::Fixed));
        return widget;
    }

    QWidget* GroupProfile::initSettings(QWidget* _parent)
    {
        auto widget = new QWidget(_parent);
        Utils::transparentBackgroundStylesheet(widget);
        Utils::emptyContentsMargins(widget);

        auto layout = Utils::emptyVLayout(widget);
        layout->setAlignment(Qt::AlignTop);

        public_ = addCheckbox(QString(), QT_TRANSLATE_NOOP("groupchats", "Public chat"), widget, layout);
        connect(public_, &SidebarCheckboxButton::checked, this, &GroupProfile::publicClicked);

        publicLabel_ = addLabel(QT_TRANSLATE_NOOP("groupchats", "Public group can be found in the search"), widget, layout, Utils::scale_value(SETTINGS_ADD_OFFSET));

        aboutLinkToChatBlock_ = new QWidget(widget);
        {
            auto aboutLinkToChatLayout = Utils::emptyVLayout(aboutLinkToChatBlock_);

            {
                auto spacer = new QWidget(widget);
                Utils::transparentBackgroundStylesheet(spacer);
                spacer->setFixedHeight(ABOUT_LINK_SPACER_HEIGHT);
                aboutLinkToChatLayout->addWidget(spacer);
            }

            const auto appName = config::get().string(config::values::product_name_full);
            const QString argName = QString::fromUtf8(appName.data(), appName.size());
            aboutLinkToChat_ = addInfoBlock(QT_TRANSLATE_NOOP("groupchats", "The link will open the group in %1").arg(argName), qsl(""), aboutLinkToChatBlock_, aboutLinkToChatLayout, Utils::scale_value(SETTINGS_ADD_OFFSET));
            aboutLinkToChat_->setHeaderLinkColor(Styling::getParameters().getColor(Styling::StyleVariable::BASE_PRIMARY));
            aboutLinkToChat_->text_->setCursorForText();
            connect(aboutLinkToChat_->text_, &TextLabel::textClicked, this, &GroupProfile::aboutLinkClicked);

            {
                auto spacer = new QWidget(widget);
                Utils::transparentBackgroundStylesheet(spacer);
                spacer->setFixedHeight(INFO_SPACER_HEIGHT);
                aboutLinkToChatLayout->addWidget(spacer);
            }

            makeNewLink_ = addText(QT_TRANSLATE_NOOP("groupchats", "Make new link"), aboutLinkToChatBlock_, aboutLinkToChatLayout, Utils::scale_value(SETTINGS_ADD_OFFSET), -1);
            makeNewLink_->setCursorForText();
            makeNewLink_->setColor(Styling::getParameters().getColor(Styling::StyleVariable::TEXT_PRIMARY));
            connect(makeNewLink_, &TextLabel::textClicked, this, &GroupProfile::makeNewLink);

            {
                auto spacer = new QWidget(widget);
                Utils::transparentBackgroundStylesheet(spacer);
                spacer->setFixedHeight(ABOUT_LINK_SPACER_HEIGHT);
                aboutLinkToChatLayout->addWidget(spacer);
            }

        }
        layout->addWidget(aboutLinkToChatBlock_);

        {
            EditNicknameWidget::FormData initData;
            initData.fixedPart_ = Utils::getDomainUrl() % ql1c('/');
            initData.groupMode_ = true;
            initData.fixedSize_ = false;
            initData.margins_ = QMargins(Utils::scale_value(SETTINGS_LEFT_OFFSET), Utils::scale_value(ABOUT_LINK_SPACER_HEIGHT), Utils::scale_value(ABOUT_LINK_RIGHT_OFFSET), 0);
            editGroupLinkWidget_ = new EditNicknameWidget(widget, initData);
            editGroupLinkWidget_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            layout->addWidget(editGroupLinkWidget_);
            editGroupLinkWidget_->hide();
        }

        joinModeration_ = addCheckbox(QString(), QT_TRANSLATE_NOOP("groupchats", "Join with Approval"), widget, layout);
        connect(joinModeration_, &SidebarCheckboxButton::checked, this, &GroupProfile::checkApplyButton);

        addLabel(QT_TRANSLATE_NOOP("groupchats", "New members are waiting for admin approval"), widget, layout, Utils::scale_value(SETTINGS_ADD_OFFSET));

        auto spacer = new QSpacerItem(0, Utils::scale_value(APPLY_SETTINGS_MARGIN), QSizePolicy::Preferred, QSizePolicy::Fixed);
        layout->addItem(spacer);

        auto hLayout = Utils::emptyHLayout();
        hLayout->setContentsMargins(Utils::scale_value(SETTINGS_LEFT_OFFSET), 0, 0, 0);
        applyChatSettings_ = new DialogButton(widget, QT_TRANSLATE_NOOP("groupchats", "Apply"));
        hLayout->addWidget(applyChatSettings_);
        hLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding));
        layout->addLayout(hLayout);

        connect(editGroupLinkWidget_, &Ui::EditNicknameWidget::ready, this, [this]() { invalidNick_ = false; checkApplyButton(); });
        connect(editGroupLinkWidget_, &Ui::EditNicknameWidget::theSame, this, [this]() { invalidNick_ = true; checkApplyButton(); });
        connect(editGroupLinkWidget_, &Ui::EditNicknameWidget::error, this, [this]() { invalidNick_ = true; checkApplyButton(); });

        connect(applyChatSettings_, &Ui::DialogButton::clicked, this, &GroupProfile::applyChatSettings);

        return widget;
    }

    QWidget* GroupProfile::initContactList(QWidget* _parent)
    {
        auto widget = new QWidget(_parent);
        auto contactListLayout = Utils::emptyVLayout(widget);

        searchWidget_ = new SearchWidget(widget);
        searchWidget_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        contactListLayout->addWidget(searchWidget_);

        contactListLayout->addSpacerItem(new QSpacerItem(0, Utils::scale_value(SPACER_MARGIN), QSizePolicy::Preferred, QSizePolicy::Fixed));

        cl_ = new Ui::ContactListWidget(widget, Logic::MEMBERS_LIST, listChatModel_);
        cl_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        cl_->setContentsMargins(0, 0, 0, 0);
        cl_->setClDelegate(delegate_);
        cl_->connectSearchWidget(searchWidget_);

        connect(cl_, &ContactListWidget::selected, this, &GroupProfile::contactSelected);
        connect(cl_, &ContactListWidget::removeClicked, this, &GroupProfile::contactRemoved);
        connect(cl_, &ContactListWidget::moreClicked, this, &GroupProfile::contactMenuRequested);
        connect(cl_, &ContactListWidget::approve, this, &GroupProfile::contactMenuApproved);

        contactListLayout->addWidget(cl_);

        approveAll_ = addButton(qsl(":/waiting_for_accept_icon"), QT_TRANSLATE_NOOP("sidebar", "Approve All"), widget, contactListLayout);
        approveAll_->setFixedHeight(Utils::scale_value(APPROVE_ALL_BUTTON_HEIGHT));
        connect(approveAll_, &SidebarButton::clicked, this, &GroupProfile::approveAllClicked);

        return widget;
    }

    QWidget* GroupProfile::initGallery(QWidget* _parent)
    {
        galleryPopup_ = new GalleryPopup();
        connect(galleryPopup_, &GalleryPopup::galleryPhotoCLicked, this, &GroupProfile::galleryPhotoCLicked);
        connect(galleryPopup_, &GalleryPopup::galleryVideoCLicked, this, &GroupProfile::galleryVideoCLicked);
        connect(galleryPopup_, &GalleryPopup::galleryFilesCLicked, this, &GroupProfile::galleryFilesCLicked);
        connect(galleryPopup_, &GalleryPopup::galleryLinksCLicked, this, &GroupProfile::galleryLinksCLicked);
        connect(galleryPopup_, &GalleryPopup::galleryPttCLicked, this, &GroupProfile::galleryPttCLicked);

        auto widget = new QWidget(_parent);
        auto layout = Utils::emptyVLayout(widget);

        gallery_ = new GalleryList(widget, QString());
        layout->addWidget(gallery_);

        return widget;
    }

    void GroupProfile::initTexts()
    {
        share_->setText(isChannel(chatInfo_) ? QT_TRANSLATE_NOOP("sidebar", "Share channel") : QT_TRANSLATE_NOOP("sidebar", "Share group"));
        settings_->setText(isChannel(chatInfo_) ? QT_TRANSLATE_NOOP("sidebar", "Channel settings") : QT_TRANSLATE_NOOP("sidebar", "Group settings"));
        addToChat_->setText(isChannel(chatInfo_) ? QT_TRANSLATE_NOOP("sidebar", "Add to channel") : QT_TRANSLATE_NOOP("sidebar", "Add to group"));
        public_->setText(isChannel(chatInfo_) ? QT_TRANSLATE_NOOP("groupchats", "Public channel") : QT_TRANSLATE_NOOP("sidebar", "Public group"));
        const auto appName = config::get().string(config::values::product_name_full);
        const QString argName = QString::fromUtf8(appName.data(), appName.size());
        aboutLinkToChat_->setHeaderText(QString(isChannel(chatInfo_) ? QT_TRANSLATE_NOOP("groupchats", "The link will open the channel in %1") : QT_TRANSLATE_NOOP("groupchats", "The link will open the group in %1")).arg(argName));
        aboutLinkToChat_->setHeaderLinkColor(Styling::getParameters().getColor(Styling::StyleVariable::BASE_PRIMARY));
        publicLabel_->setText(isChannel(chatInfo_) ? QT_TRANSLATE_NOOP("groupchats", "The public channel can be found in the search and its link will be available to all members") : QT_TRANSLATE_NOOP("groupchats", "The public group can be found in the search and its link will be available to all members"));
    }

    void GroupProfile::updateCloseIcon()
    {
        const auto headerIconSize = QSize(ICON_SIZE, ICON_SIZE);
        if (frameCountMode_ == FrameCountMode::_1 || stackedWidget_->currentIndex() != main)
            closeButton_->setDefaultImage(qsl(":/controls/back_icon"), QColor(), headerIconSize);
        else
            closeButton_->setDefaultImage(qsl(":/history_icon"), QColor(), headerIconSize);
        Styling::Buttons::setButtonDefaultColors(closeButton_);
    }

    void GroupProfile::updatePinButton()
    {
        auto isPinned = Logic::getRecentsModel()->isFavorite(currentAimId_);
        pin_->setIcon(Utils::renderSvgScaled(isPinned ? qsl(":/unpin_chat_icon") : qsl(":/pin_chat_icon"), QSize(ICON_SIZE, ICON_SIZE), Styling::getParameters().getColor(Styling::StyleVariable::BASE_SECONDARY)));
        pin_->setText(isPinned ? QT_TRANSLATE_NOOP("sidebar", "Unpin") : QT_TRANSLATE_NOOP("sidebar", "Pin"));
        pin_->setVisible(!Logic::getRecentsModel()->isUnimportant(currentAimId_));
    }

    void GroupProfile::loadChatInfo()
    {
        Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
        collection.set_value_as_qstring("aimid", currentAimId_);

        if (currentAimId_.isEmpty() || Logic::getContactListModel()->youAreNotAMember(currentAimId_))
        {
            const auto stamp = Logic::getContactListModel()->getChatStamp(currentAimId_);
            if (!stamp.isEmpty())
                collection.set_value_as_qstring("stamp", stamp);
            else
                return;
        }

        collection.set_value_as_int("limit", SHORT_MEMBERS_COUNT);
        Ui::GetDispatcher()->post_message_to_core("chats/info/get", collection.get());
    }

    void GroupProfile::closeGallery()
    {
        gallery_->markClosed();
    }

    void GroupProfile::changeTab(int _tab)
    {
        switch (_tab)
        {
        case main:
            titleBar_->setTitle(QT_TRANSLATE_NOOP("sidebar", "Information"));
            titleBar_->setArrowVisible(false);
            break;

        case settings:
            titleBar_->setTitle(isChannel(chatInfo_) ? QT_TRANSLATE_NOOP("sidebar", "Channel settings") : QT_TRANSLATE_NOOP("sidebar", "Group settings"));
            titleBar_->setArrowVisible(false);
            editGroupLinkWidget_->setVisible(chatInfo_ && chatInfo_->Public_);
            if (chatInfo_)
                editGroupLinkWidget_->setNick(chatInfo_->Stamp_);
            editGroupLinkWidget_->clearHint();
            aboutLinkToChatBlock_->setVisible(chatInfo_ && !chatInfo_->Public_);

            public_->blockSignals(true);
            public_->setChecked(chatInfo_->Public_);
            public_->blockSignals(false);

            joinModeration_->blockSignals(true);
            joinModeration_->setChecked(chatInfo_->ApprovedJoin_);
            joinModeration_->blockSignals(false);

            applyChatSettings_->setEnabled(false);
            break;

        case list:
            titleBar_->setArrowVisible(false);
            break;

        case gallery:
            titleBar_->setArrowVisible(true);
            break;
        }

        stackedWidget_->setCurrentIndex(_tab);
        searchWidget_->setNeedClear(_tab != list);
        searchWidget_->searchCompleted();

        updateCloseIcon();
        updateRegim();

        if (_tab == main)
            editButton_->setVisible(isYouAdminOrModer(chatInfo_) || !isChatControlled(chatInfo_));
        else
            editButton_->setVisible(false);
    }

    void GroupProfile::changeGalleryPage(int _page)
    {
        switch (_page)
        {
        case photo_and_video:
            titleBar_->setTitle(QT_TRANSLATE_NOOP("sidebar", "Photo and video"));
            gallery_->setTypes({ ql1s("image"), ql1s("video") });
            gallery_->setContentWidget(new ImageVideoList(gallery_, MediaContentWidget::ImageVideo, "Photo+Video"));
            break;

        case video:
            titleBar_->setTitle(QT_TRANSLATE_NOOP("sidebar", "Video"));
            gallery_->setType(ql1s("video"));
            gallery_->setContentWidget(new ImageVideoList(gallery_, MediaContentWidget::Video, "Video"));
            break;

        case files:
            titleBar_->setTitle(QT_TRANSLATE_NOOP("sidebar", "Files"));
            gallery_->setTypes({ ql1s("file"), ql1s("audio") });
            gallery_->setContentWidget(new FilesList(gallery_));
            break;

        case links:
            titleBar_->setTitle(QT_TRANSLATE_NOOP("sidebar", "Links"));
            gallery_->setType(ql1s("link"));
            gallery_->setContentWidget(new LinkList(gallery_));
            break;

        case ptt:
            titleBar_->setTitle(QT_TRANSLATE_NOOP("sidebar", "Voice messages"));
            gallery_->setType(ql1s("ptt"));
            gallery_->setContentWidget(new PttList(gallery_));
            break;

        default:
            assert(false);
        }

        gallery_->initFor(currentAimId_);
        currentGalleryPage_ = _page;
    }

    void GroupProfile::updateRegim()
    {
        if (!chatInfo_)
            return;

        if (stackedWidget_->currentIndex() == list)
        {
            switch (currentListPage_)
            {
            case pending:
                delegate_->setRegim(Logic::MembersWidgetRegim::PENDING_MEMBERS);
                return;

            case blocked:
                delegate_->setRegim(isYouAdminOrModer(chatInfo_) ? Logic::MembersWidgetRegim::MEMBERS_LIST : Logic::MembersWidgetRegim::CONTACT_LIST);
                return;

            default:
                break;
            }
        }

        delegate_->setRegim(isYouAdminOrModer(chatInfo_) ? Logic::MembersWidgetRegim::ADMIN_MEMBERS : Logic::MembersWidgetRegim::CONTACT_LIST);
    }

    void GroupProfile::updateChatControls()
    {
        if (!chatInfo_)
            return;

        const auto chatInCL = Logic::getContactListModel()->contains(currentAimId_);
        const auto isIgnored = Logic::getIgnoreModel()->contains(currentAimId_);
        const auto isUnimportant = Logic::getRecentsModel()->isUnimportant(currentAimId_);
        const auto youBlocked = chatInfo_->YouBlocked_;
        const auto youPending = chatInfo_->YouPending_;
        const auto youNotMember = chatInfo_->YourRole_.isEmpty();
        const auto justAMember = !isIgnored && !youBlocked && !youNotMember && !youPending;
        const auto chatIsChannel = isChannel(chatInfo_);

        const auto makeIcon = [](const auto& _path, const QSize& _iconSize = QSize(BTN_ICON_SIZE, BTN_ICON_SIZE))
        {
            return Utils::renderSvgScaled(_path, _iconSize, Styling::getParameters().getColor(Styling::StyleVariable::TEXT_SOLID_PERMANENT));
        };

        if (isIgnored)
        {
            mainAction_ = unblock;
            mainActionButton_->setIcon(makeIcon(qsl(":/unlock_icon")));
            mainActionButton_->setText(QT_TRANSLATE_NOOP("sidebar", "Unblock"));
            mainActionButton_->show();
        }
        else if (youBlocked)
        {
            groupStatus_->setText(QT_TRANSLATE_NOOP("sidebar", "You can't read or write messages in this group because you are blocked"));
            groupStatus_->show();
        }
        else if (youPending)
        {
            groupStatus_->setText(QT_TRANSLATE_NOOP("sidebar", "The join request has been sent to administrator"));
            groupStatus_->show();
        }
        else if (youNotMember)
        {
            if (!chatInfo_->Stamp_.isEmpty())
            {
                mainAction_ = join;
                mainActionButton_->setIcon(makeIcon(qsl(":/input/add")));
                mainActionButton_->setText(chatIsChannel ? QT_TRANSLATE_NOOP("sidebar", "Subscribe") : QT_TRANSLATE_NOOP("sidebar", "Join"));
                mainActionButton_->show();
            }
            else
            {
                mainActionButton_->hide();
            }
        }
        else
        {
            mainAction_ = invite;
            mainActionButton_->setIcon(makeIcon(qsl(":/add_user_icon"), QSize(ICON_SIZE, ICON_SIZE)));
            mainActionButton_->setText(chatIsChannel ? QT_TRANSLATE_NOOP("sidebar", "Add to channel") : QT_TRANSLATE_NOOP("sidebar", "Add to chat"));
            mainActionButton_->setVisible(frameCountMode_ == FrameCountMode::_1);
        }

        link_->setVisible(chatInfo_->Public_ || isYouAdminOrModer(chatInfo_));
        if (link_->isVisible())
        {
            link_->text_->clearMenuActions();
            if (chatIsChannel)
            {
                link_->text_->addMenuAction(qsl(":/context_menu/forward"), QT_TRANSLATE_NOOP("context_menu", "Share channel link"), makeCopyData(qsl("share")));
                link_->text_->addMenuAction(qsl(":/context_menu/copy"), QT_TRANSLATE_NOOP("context_menu", "Copy channel link"), makeCopyData(qsl("copy_link")));
            }
            else
            {
                link_->text_->addMenuAction(qsl(":/context_menu/forward"), QT_TRANSLATE_NOOP("context_menu", "Share group link"), makeCopyData(qsl("share")));
                link_->text_->addMenuAction(qsl(":/context_menu/copy"), QT_TRANSLATE_NOOP("context_menu", "Copy group link"), makeCopyData(qsl("copy_link")));
            }
        }

        share_->setVisible(chatInfo_->Public_ || isYouAdminOrModer(chatInfo_));
        notifications_->setVisible(chatInCL || (!isIgnored && !youNotMember));
        settings_->setVisible(isYouAdminOrModer(chatInfo_));
        secondSpacer_->setVisible(share_->isVisible() || notifications_->isVisible() || settings_->isVisible());
        galleryWidget_->setVisible(!galleryIsEmpty_ && !youPending && !youNotMember);
        thirdSpacer_->setVisible(galleryWidget_->isVisible());
        membersWidget_->setVisible(justAMember && (!chatIsChannel || isYouAdminOrModer(chatInfo_)));
        about_->setHeaderText(chatIsChannel ? QT_TRANSLATE_NOOP("siderbar", "About the channel") : QT_TRANSLATE_NOOP("siderbar", "About the group"));
        about_->setVisible(!chatInfo_->About_.isEmpty());
        rules_->setVisible(!chatInfo_->Rules_.isEmpty());
        fourthSpacer_->setVisible(justAMember && (!chatIsChannel || isYouAdminOrModer(chatInfo_)));
        pin_->setVisible(!youPending && !youNotMember && !isUnimportant);
        theme_->setVisible(chatInCL || (!youPending && !youNotMember));
        fifthSpacer_->setVisible(chatInCL || (!youPending && !youNotMember));
        clearHistory_->setVisible(chatInCL || (!youPending && !youNotMember));
        block_->setVisible(chatInCL || youBlocked || justAMember);
        report_->setVisible(chatInCL || youBlocked || justAMember);
        leave_->setVisible(chatInCL || youBlocked || justAMember);
        infoSpacer_->setVisible(mainActionButton_->isVisible() || about_->isVisible() || link_->isVisible());
        editButton_->setVisible(stackedWidget_->currentIndex() == main && (isYouAdminOrModer(chatInfo_) || !isChatControlled(chatInfo_)));
    }

    void GroupProfile::blockUser(const QString& aimId, ActionType _action)
    {
        auto model = stackedWidget_->currentIndex() == list ? listChatModel_ : shortChatModel_;
        if (!model->contains(aimId))
            return;

        bool confirmed = false, removeMessages = false;
        if (_action == ActionType::POSITIVE)
        {
            auto w = new Ui::BlockAndDeleteWidget(nullptr, Logic::GetFriendlyContainer()->getFriendly(aimId), currentAimId_);
            GeneralDialog generalDialog(w, Utils::InterConnector::instance().getMainWindow());
            generalDialog.addLabel(QT_TRANSLATE_NOOP("block_and_delete", "Block and delete?"));
            generalDialog.addButtonsPair(QT_TRANSLATE_NOOP("block_and_delete", "Cancel"), QT_TRANSLATE_NOOP("block_and_delete", "Yes"), true);
            confirmed = generalDialog.showInCenter();
            removeMessages = w->needToRemoveMessages();
        }
        else
        {
            confirmed = Utils::GetConfirmationWithTwoButtons(
                QT_TRANSLATE_NOOP("popup_window", "Cancel"),
                QT_TRANSLATE_NOOP("popup_window", "Yes"),
                QT_TRANSLATE_NOOP("popup_window", "Are you sure you want to unblock user?"),
                Logic::GetFriendlyContainer()->getFriendly(aimId),
                nullptr);
        }

        if (confirmed)
        {
            Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
            collection.set_value_as_qstring("aimid", currentAimId_);
            collection.set_value_as_qstring("contact", aimId);
            collection.set_value_as_bool("block", _action == ActionType::POSITIVE);
            collection.set_value_as_bool("remove_messages", removeMessages);
            Ui::GetDispatcher()->post_message_to_core("chats/block", collection.get());
        }

        if (_action == ActionType::NEGATIVE)
        {
            if (chatInfo_ && chatInfo_->BlockedCount_ == 1)
                changeTab(main);

            model->loadBlocked();
        }
    }

    void GroupProfile::readOnly(const QString& _aimId, ActionType _action)
    {
        auto model = stackedWidget_->currentIndex() == list ? listChatModel_ : shortChatModel_;
        if (!model->contains(_aimId))
            return;

        const auto confirmed = Utils::GetConfirmationWithTwoButtons(
            QT_TRANSLATE_NOOP("popup_window", "Cancel"),
            QT_TRANSLATE_NOOP("popup_window", "Yes"),
            _action == ActionType::POSITIVE ? QT_TRANSLATE_NOOP("popup_window", "Are you sure you want to ban user to write in this chat?") : QT_TRANSLATE_NOOP("popup_window", "Are you sure you want to allow user to write in this chat?"),
            Logic::GetFriendlyContainer()->getFriendly(_aimId),
            nullptr);

        if (confirmed)
        {
            Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
            collection.set_value_as_qstring("aimid", currentAimId_);
            collection.set_value_as_qstring("contact", _aimId);
            collection.set_value_as_qstring("role", _action == ActionType::POSITIVE ? qsl("readonly") : qsl("member"));
            Ui::GetDispatcher()->post_message_to_core("chats/role/set", collection.get());
        }
    }

    void GroupProfile::changeRole(const QString& aimId, ActionType _action)
    {
        auto model = stackedWidget_->currentIndex() == list ? listChatModel_ : shortChatModel_;
        if (!model->contains(aimId))
            return;

        const auto confirmed = Utils::GetConfirmationWithTwoButtons(
            QT_TRANSLATE_NOOP("popup_window", "Cancel"),
            QT_TRANSLATE_NOOP("popup_window", "Yes"),
            _action == ActionType::POSITIVE ? QT_TRANSLATE_NOOP("popup_window", "Are you sure you want to make user admin in this chat?") : QT_TRANSLATE_NOOP("popup_window", "Are you sure you want to revoke admin role?"),
            Logic::GetFriendlyContainer()->getFriendly(aimId),
            nullptr);

        if (confirmed)
        {
            Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
            collection.set_value_as_qstring("aimid", currentAimId_);
            collection.set_value_as_qstring("contact", aimId);
            collection.set_value_as_qstring("role", _action == ActionType::POSITIVE ? qsl("moder") : (isChannel(chatInfo_) ? qsl("readonly") : qsl("member")));
            Ui::GetDispatcher()->post_message_to_core("chats/role/set", collection.get());
        }
    }

    void GroupProfile::chatInfo(qint64, const std::shared_ptr<Data::ChatInfo>& _chat_info, const int _requestMembersLimit)
    {
        if (!isActive() || _chat_info->AimId_ != currentAimId_)
            return;

        if (!chatInfo_ || chatInfo_->AimId_ != _chat_info->AimId_ || chatInfo_->MembersCount_ != _chat_info->MembersCount_ || forceMembersRefresh_)
        {
            forceMembersRefresh_ = false;
            shortMembersList_->clearCache();
            shortChatModel_->updateInfo(_chat_info);
        }

        chatInfo_ = _chat_info;
        initTexts();

        info_->setInfo(QString::number(_chat_info->MembersCount_) % ql1c(' ') %
            Utils::GetTranslator()->getNumberString(_chat_info->MembersCount_,
                QT_TRANSLATE_NOOP3("sidebar", "member", "1"),
                QT_TRANSLATE_NOOP3("sidebar", "members", "2"),
                QT_TRANSLATE_NOOP3("sidebar", "members", "5"),
                QT_TRANSLATE_NOOP3("sidebar", "members", "21")
            ));

        addToChat_->setText(isChannel(_chat_info) ? QT_TRANSLATE_NOOP("sidebar", "Add to channel") : QT_TRANSLATE_NOOP("sidebar", "Add to chat"));

        if (!_chat_info->About_.isEmpty())
            about_->setText(_chat_info->About_);

        if (!_chat_info->Rules_.isEmpty())
            rules_->setText(_chat_info->Rules_);

        link_->setText(Utils::getDomainUrl() % ql1c('/') % _chat_info->Stamp_, Styling::getParameters().getColor(Styling::StyleVariable::TEXT_PRIMARY));
        link_->setVisible(chatInfo_->Public_ || isYouAdminOrModer(chatInfo_));

        infoSpacer_->setVisible(mainActionButton_->isVisible() || about_->isVisible() || link_->isVisible());
        share_->setVisible(chatInfo_->Public_ || isYouAdminOrModer(chatInfo_));

        members_->setMembersCount(_chat_info->MembersCount_);
        allMembers_->setCounter(_chat_info->MembersCount_);
        approveAll_->setCounter(_chat_info->PendingCount_, false);

        settings_->setVisible(isYouAdminOrModer(chatInfo_));
        if (stackedWidget_->currentIndex() == main)
            editButton_->setVisible(isYouAdminOrModer(chatInfo_) || !isChatControlled(_chat_info));
        else
            editButton_->hide();

        if (isYouAdminOrModer(_chat_info))
        {
            pendings_->setCounter(_chat_info->PendingCount_);
            blockedMembers_->setCounter(_chat_info->BlockedCount_);
        }
        else
        {
            pendings_->hide();
            blockedMembers_->hide();
        }

        if (isChannel(_chat_info))
        {
            admins_->setCounter(_chat_info->AdminsCount_, false);
            admins_->setVisible(isYouAdminOrModer(_chat_info));
        }
        else
        {
            admins_->setCounter(_chat_info->AdminsCount_);
        }

        auto setCheckedBlocking = [](SidebarCheckboxButton* _checkbox, const bool _isChecked)
        {
            QSignalBlocker sb(_checkbox);
            _checkbox->setChecked(_isChecked);
        };

        setCheckedBlocking(public_, _chat_info->Public_);
        setCheckedBlocking(joinModeration_, _chat_info->ApprovedJoin_);

        aboutLinkToChat_->setText(Utils::getDomainUrl() % ql1c('/') % chatInfo_->Stamp_);
        editGroupLinkWidget_->setNick(chatInfo_->Stamp_);

        editGroupLinkWidget_->setVisible(chatInfo_->Public_);
        editGroupLinkWidget_->clearHint();
        aboutLinkToChatBlock_->setVisible(!chatInfo_->Public_);

        checkApplyButton();

        updateRegim();
        updateChatControls();
    }

    void GroupProfile::chatInfoFailed(qint64 _seq, core::group_chat_info_errors _errorCode, const QString& _aimId)
    {
        if (!isActive() || _aimId != currentAimId_)
            return;

        if (_errorCode == core::group_chat_info_errors::not_in_chat || _errorCode == core::group_chat_info_errors::blocked)
        {
            if (chatInfo_)
            {
                chatInfo_->YourRole_.clear();
            }
            else
            {
                chatInfo_ = std::make_shared<Data::ChatInfo>();
                chatInfo_->Controlled_ = true;
            }

            info_->setInfo(QT_TRANSLATE_NOOP("groupchats", "You are not a member of this chat"));

            if (_errorCode == core::group_chat_info_errors::blocked)
                chatInfo_->YouBlocked_ = true;
            updateChatControls();
        }
    }

    void GroupProfile::dialogGalleryState(const QString& _aimId, const Data::DialogGalleryState& _state)
    {
        if (!isActive() || _aimId != currentAimId_)
            return;

        galleryIsEmpty_ = _state.isEmpty();

        galleryPhoto_->setCounter(_state.images_count_ + _state.videos_count);
        galleryVideo_->setCounter(_state.videos_count);
        galleryFiles_->setCounter(_state.files_count);
        galleryLinks_->setCounter(_state.links_count);
        galleryPtt_->setCounter(_state.ptt_count);
        galleryPopup_->setCounters(_state.images_count_ + _state.videos_count, _state.videos_count, _state.files_count, _state.links_count, _state.ptt_count);

        galleryWidget_->setVisible(!galleryIsEmpty_);
        thirdSpacer_->setVisible(!galleryIsEmpty_);

        if (stackedWidget_->currentIndex() == gallery)
        {
            switch (currentGalleryPage_)
            {
            case photo_and_video:
                if (_state.images_count_ + _state.videos_count != 0)
                    return;
                break;

            case video:
                if (_state.videos_count != 0)
                    return;
                break;

            case files:
                if (_state.files_count != 0)
                    return;
                break;

            case links:
                if (_state.links_count != 0)
                    return;
                break;

            case ptt:
                if (_state.ptt_count != 0)
                    return;
                break;
            }

            closeGallery();
            changeTab(main);
        }
    }

    void GroupProfile::favoriteChanged(const QString _aimid)
    {
        if (!isActive() || _aimid != currentAimId_)
            return;

        updatePinButton();
    }

    void GroupProfile::unimportantChanged(const QString _aimid)
    {
        if (!isActive() || _aimid != currentAimId_)
            return;

        updatePinButton();
    }

    void GroupProfile::modChatParamsResult(int _error)
    {
        applyChatSettings_->setEnabled(_error != 0);
    }

    void GroupProfile::suggestGroupNickResult(const QString& _nick)
    {
        auto nick = _nick.isEmpty() ? chatInfo_->Stamp_ : _nick;
        aboutLinkToChat_->setText(Utils::getDomainUrl() % ql1c('/') % nick);
        editGroupLinkWidget_->setNick(nick);

        checkApplyButton();
    }

    void GroupProfile::share()
    {
        if (!chatInfo_)
            return;

        forwardMessage(ql1s("https://") % Utils::getDomainUrl() % ql1c('/') % chatInfo_->Stamp_, QString(), QString(), false);
    }

    void GroupProfile::shareClicked()
    {
        share();
        GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::profilescr_sharecontact_action, { { "chat_type", Utils::chatTypeByAimId(currentAimId_) } });
    }

    void GroupProfile::copy(const QString& _text)
    {
        QApplication::clipboard()->setText(_text);
    }

    void GroupProfile::notificationsChecked(bool _checked)
    {
        Logic::getRecentsModel()->muteChat(currentAimId_, !_checked);
        GetDispatcher()->post_stats_to_core(_checked ? core::stats::stats_event_names::unmute : core::stats::stats_event_names::mute_sidebar);
        GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::profilescr_notifications_event, { { "chat_type", Utils::chatTypeByAimId(currentAimId_) }, { "do" , _checked ? "enable" : "mute" } });
    }

    void GroupProfile::settingsClicked()
    {
        checkApplyButton();
        changeTab(settings);
        GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::profilescr_settings_event, { { "chat_type", Utils::chatTypeByAimId(currentAimId_) } });
    }

    void GroupProfile::galleryPhotoCLicked()
    {
        galleryPopup_->hide();
        changeTab(gallery);
        changeGalleryPage(photo_and_video);
    }

    void GroupProfile::galleryVideoCLicked()
    {
        galleryPopup_->hide();
        changeTab(gallery);
        changeGalleryPage(video);
    }

    void GroupProfile::galleryFilesCLicked()
    {
        galleryPopup_->hide();
        changeTab(gallery);
        changeGalleryPage(files);
    }

    void GroupProfile::galleryLinksCLicked()
    {
        galleryPopup_->hide();
        changeTab(gallery);
        changeGalleryPage(links);
    }

    void GroupProfile::galleryPttCLicked()
    {
        galleryPopup_->hide();
        changeTab(gallery);
        changeGalleryPage(ptt);
    }

    void GroupProfile::searchMembersClicked()
    {
        currentListPage_ = all;
        changeTab(list);
        titleBar_->setTitle(QT_TRANSLATE_NOOP("sidebar", "Members"));
        listChatModel_->loadAllMembers();
        approveAll_->hide();
        searchWidget_->setFocus();
    }

    void GroupProfile::addToChatClicked()
    {
        Logic::ChatContactsModel membersModel;
        membersModel.loadChatContacts(currentAimId_);

        SelectContactsWidget selectMembersDialog(&membersModel, Logic::MembersWidgetRegim::SELECT_MEMBERS,
                isChannel(chatInfo_) ? QT_TRANSLATE_NOOP("sidebar", "Add to channel") : QT_TRANSLATE_NOOP("sidebar", "Add to chat"),
                QT_TRANSLATE_NOOP("popup_window", "Done"), this);

        if (selectMembersDialog.show() == QDialog::Accepted)
        {
            GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::adtochatscr_adding_action, { { "counter", Utils::averageCount(Logic::getContactListModel()->GetCheckedContacts().size()) }, {"from", "profile" } });
            postAddChatMembersFromCLModelToCore(currentAimId_);
            GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::chats_add_member_sidebar);
        }
        else
        {
            Logic::getContactListModel()->clearChecked();
        }

        GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::profilescr_adding_action, { { "chat_type", Utils::chatTypeByAimId(currentAimId_) }, {"from", "head" } });
    }

    void GroupProfile::pendingsClicked()
    {
        currentListPage_ = pending;
        changeTab(list);
        titleBar_->setTitle(QT_TRANSLATE_NOOP("sidebar", "Waiting for approval"));
        listChatModel_->loadPending();
        approveAll_->show();
        searchWidget_->setFocus();
    }

    void GroupProfile::allMembersClicked()
    {
        currentListPage_ = all;
        changeTab(list);
        titleBar_->setTitle(QT_TRANSLATE_NOOP("sidebar", "Members"));
        listChatModel_->loadAllMembers();
        approveAll_->hide();
        searchWidget_->setFocus();
        GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::profilescr_members_action, { { "chat_type", Utils::chatTypeByAimId(currentAimId_) } });
    }

    void GroupProfile::adminsClicked()
    {
        currentListPage_ = admins;
        changeTab(list);
        titleBar_->setTitle(QT_TRANSLATE_NOOP("sidebar", "Admins"));
        listChatModel_->loadAdmins();
        approveAll_->hide();
        searchWidget_->setFocus();
    }

    void GroupProfile::blockedMembersClicked()
    {
        currentListPage_ = blocked;
        changeTab(list);
        titleBar_->setTitle(QT_TRANSLATE_NOOP("sidebar", "Blocked people"));
        listChatModel_->loadBlocked();
        approveAll_->hide();
        searchWidget_->setFocus();
        GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::profilescr_blocked_action, { { "chat_type", Utils::chatTypeByAimId(currentAimId_) } });
    }

    void GroupProfile::pinClicked()
    {
        Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
        collection.set_value_as_qstring("contact", currentAimId_);
        Ui::GetDispatcher()->post_message_to_core(Logic::getRecentsModel()->isFavorite(currentAimId_) ? std::string_view("unfavorite") : std::string_view("favorite"), collection.get());

        emit Utils::InterConnector::instance().closeAnySemitransparentWindow({ Utils::CloseWindowInfo::Initiator::Unknown, Utils::CloseWindowInfo::Reason::Keep_Sidebar });
    }

    void GroupProfile::themeClicked()
    {
        showWallpaperSelectionDialog(currentAimId_);
    }

    void GroupProfile::clearHistoryClicked()
    {
        closeGallery();

        const auto confirmed = Utils::GetConfirmationWithTwoButtons(
            QT_TRANSLATE_NOOP("popup_window", "Cancel"),
            QT_TRANSLATE_NOOP("popup_window", "Yes"),
            isChannel(chatInfo_) ? QT_TRANSLATE_NOOP("popup_window", "Are you sure you want to clear the channel history?") : QT_TRANSLATE_NOOP("popup_window", "Are you sure you want to erase chat history?"),
            Logic::GetFriendlyContainer()->getFriendly(currentAimId_),
            nullptr);

        if (confirmed)
        {
            eraseHistory(currentAimId_);
            GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::profilescr_blockbuttons_action, { { "do", "spam" },{ "chat_type", Utils::chatTypeByAimId(currentAimId_) } });
        }
    }

    void GroupProfile::blockClicked()
    {
        closeGallery();

        if (Logic::getContactListModel()->ignoreContactWithConfirm(currentAimId_))
        {
            GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::ignore_contact, { { "From", "Sidebar" } });
            Utils::InterConnector::instance().setSidebarVisible(false);
            GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::profilescr_blockbuttons_action, { { "do", "block" },{ "chat_type", Utils::chatTypeByAimId(currentAimId_) } });
        }
    }

    void GroupProfile::reportCliked()
    {
        closeGallery();

        if (Ui::ReportContact(currentAimId_, Logic::GetFriendlyContainer()->getFriendly(currentAimId_)))
        {
            Logic::getContactListModel()->ignoreContact(currentAimId_, true);
            GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::spam_contact, { { "From", "Sidebar" } });
            GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::profilescr_blockbuttons_action, { { "do", "spam" },{ "chat_type", Utils::chatTypeByAimId(currentAimId_) } });
        }
    }

    void GroupProfile::leaveClicked()
    {
        closeGallery();

        auto confirmed = Utils::GetConfirmationWithTwoButtons(
            QT_TRANSLATE_NOOP("popup_window", "Cancel"),
            QT_TRANSLATE_NOOP("popup_window", "Yes"),
            isChannel(chatInfo_) ? QT_TRANSLATE_NOOP("popup_window", "Are you sure you want to leave channel?") : QT_TRANSLATE_NOOP("popup_window", "Are you sure you want to leave chat?"),
            Logic::GetFriendlyContainer()->getFriendly(currentAimId_),
            nullptr);

        if (confirmed)
        {
            Logic::getContactListModel()->removeContactFromCL(currentAimId_);
            GetDispatcher()->getVoipController().setDecline(currentAimId_.toUtf8().constData(), false);
            GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::delete_contact, { { "From", "Sidebar" } });
        }
    }

    void GroupProfile::linkClicked()
    {
        if (!chatInfo_)
            return;

        copy(ql1s("https://") % Utils::getDomainUrl() % ql1c('/') % chatInfo_->Stamp_);
        Utils::showToastOverMainWindow(QT_TRANSLATE_NOOP("sidebar", "Link copied"), Utils::scale_value(TOAST_OFFSET));
        Ui::GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::profilescr_groupurl_action, { { "chat_type", Utils::chatTypeByAimId(currentAimId_) }, { "do", "copy_url" } });
    }

    void GroupProfile::aboutLinkClicked()
    {
        if (applyChatSettings_->isEnabled())
            return;

        copy(ql1s("https://") % aboutLinkToChat_->text_->getText());
        Utils::showToastOverMainWindow(QT_TRANSLATE_NOOP("sidebar", "Link copied"), Utils::scale_value(TOAST_OFFSET));
    }

    void GroupProfile::linkCopy()
    {
        if (!chatInfo_)
            return;

        copy(ql1s("https://") % Utils::getDomainUrl() % ql1c('/') % chatInfo_->Stamp_);
        Utils::showToastOverMainWindow(QT_TRANSLATE_NOOP("sidebar", "Link copied"), Utils::scale_value(TOAST_OFFSET));
        Ui::GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::profilescr_groupurl_action, { { "chat_type", Utils::chatTypeByAimId(currentAimId_) }, { "do", "copy_url" } });
    }

    void GroupProfile::linkShare()
    {
        share();
        Ui::GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::profilescr_groupurl_action, { { "chat_type", Utils::chatTypeByAimId(currentAimId_) }, { "do", "share" } });
    }

    void GroupProfile::mainActionClicked()
    {
        if (mainAction_ == invite)
        {
            addToChatClicked();
        }
        else if (mainAction_ == join)
        {
            if (chatInfo_)
                Logic::getContactListModel()->joinLiveChat(chatInfo_->Stamp_, true);
        }
        else if (mainAction_ == unblock)
        {
            const auto confirmed = Utils::GetConfirmationWithTwoButtons(
                QT_TRANSLATE_NOOP("popup_window", "Cancel"),
                QT_TRANSLATE_NOOP("popup_window", "Yes"),
                QT_TRANSLATE_NOOP("popup_window", "Are you sure you want to delete user from ignore list?"),
                Logic::GetFriendlyContainer()->getFriendly(currentAimId_),
                nullptr);

            if (confirmed)
                Logic::getContactListModel()->ignoreContact(currentAimId_, false);

            GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::profilescr_unlock_action, { { "chat_type", Utils::chatTypeByAimId(currentAimId_) } });
        }
    }

    void GroupProfile::makeNewLink()
    {
        if (!chatInfo_)
            return;

        if (Utils::getDomainUrl() % ql1c('/') % chatInfo_->Stamp_ == aboutLinkToChat_->text_->getText())
        {
            const QString text = isChannel(chatInfo_) ? QT_TRANSLATE_NOOP("popup_window", "If you generate a new link to the channel the old one will stop working") : QT_TRANSLATE_NOOP("popup_window", "If you generate a new link to the group the old one will stop working");
            auto confirm = Utils::GetConfirmationWithTwoButtons(
                QT_TRANSLATE_NOOP("popup_window", "Cancel"),
                QT_TRANSLATE_NOOP("popup_window", "Generate"),
                text,
                QT_TRANSLATE_NOOP("popup_window", "The old link will stop working"),
                nullptr);

            if (!confirm)
            {
                return;
            }
        }

        Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
        collection.set_value_as_qstring("sn", currentAimId_);
        collection.set_value_as_bool("public", public_->isChecked());
        Ui::GetDispatcher()->post_message_to_core("suggest_group_nick", collection.get());
    }

    void GroupProfile::contactSelected(const QString& _aimid)
    {
        Utils::InterConnector::instance().showSidebar(_aimid);
        GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::profile_open, { { "From", "Members_List" } });
        GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::profilescr_chattomembers_action, { { "chat_type", Utils::chatTypeByAimId(currentAimId_) } });
    }

    void GroupProfile::contactRemoved(const QString& _aimid)
    {
        blockUser(_aimid, stackedWidget_->currentIndex() == list && currentListPage_ == blocked ? ActionType::NEGATIVE : ActionType::POSITIVE);
    }

    void GroupProfile::contactMenuRequested(const QString& _aimid)
    {
        auto menu = new ContextMenu(this);
        auto model = stackedWidget_->currentIndex() == list ? listChatModel_ : shortChatModel_;
        if (model->contains(_aimid))
        {
            if (MyInfo()->aimId() == _aimid)
            {
                menu->addActionWithIcon(
                    qsl(":/exit_icon"),
                    QT_TRANSLATE_NOOP("sidebar", "Leave and delete"),
                    makeData(qsl("leave"), _aimid));
            }
            else
            {
                const auto role = model->getMemberItem(_aimid)->getRole();
                if (role != ql1s("admin") && isYouAdmin(chatInfo_))
                {
                    if (role == ql1s("moder"))
                        menu->addActionWithIcon(
                            qsl(":/context_menu/admin_off"),
                            QT_TRANSLATE_NOOP("sidebar", "Revoke admin role"),
                            makeData(qsl("revoke_admin"), _aimid));
                    else
                        menu->addActionWithIcon(
                            qsl(":/context_menu/admin"),
                            QT_TRANSLATE_NOOP("sidebar", "Assign admin role"),
                            makeData(qsl("make_admin"), _aimid));
                }

                menu->addActionWithIcon(
                    qsl(":/context_menu/profile"),
                    QT_TRANSLATE_NOOP("sidebar", "Profile"),
                    makeData(qsl("profile"), _aimid));

                if (isYouAdminOrModer(chatInfo_))
                {
                    if (role == ql1s("member"))
                        menu->addActionWithIcon(
                            qsl(":/context_menu/readonly"),
                            QT_TRANSLATE_NOOP("sidebar", "Ban to write"),
                            makeData(qsl("readonly"), _aimid));
                    else if (role == ql1s("readonly"))
                        menu->addActionWithIcon(
                            qsl(":/context_menu/readonly_off"),
                            QT_TRANSLATE_NOOP("sidebar", "Allow to write"),
                            makeData(qsl("revoke_readonly"), _aimid));
                }

                if (role != ql1s("admin") && role != ql1s("moder"))
                    menu->addActionWithIcon(
                        qsl(":/context_menu/block"),
                        QT_TRANSLATE_NOOP("sidebar", "Block and delete"),
                        makeData(qsl("block"), _aimid));

                if (Features::clRemoveContactsAllowed())
                {
                    menu->addSeparator();
                    menu->addActionWithIcon(
                        qsl(":/context_menu/spam"),
                        QT_TRANSLATE_NOOP("sidebar", "Report"),
                        makeData(qsl("spam"), _aimid));
                }
            }
        }
        menu->invertRight(true);
        connect(menu, &ContextMenu::triggered, this, &GroupProfile::memberMenu, Qt::QueuedConnection);
        connect(menu, &ContextMenu::triggered, menu, &ContextMenu::deleteLater, Qt::QueuedConnection);
        connect(menu, &ContextMenu::aboutToHide, menu, &ContextMenu::deleteLater, Qt::QueuedConnection);
        menu->popup(QCursor::pos());
    }

    void GroupProfile::contactMenuApproved(const QString& _aimid, bool _approve)
    {
        Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
        collection.set_value_as_qstring("aimid", currentAimId_);
        core::ifptr<core::iarray> contacts_array(collection->create_array());
        contacts_array->reserve(1);
        contacts_array->push_back(collection.create_qstring_value(_aimid).get());
        collection.set_value_as_array("contacts", contacts_array.get());
        collection.set_value_as_bool("approve", _approve);
        Ui::GetDispatcher()->post_message_to_core("chats/pending/resolve", collection.get());

        if (chatInfo_ && chatInfo_->PendingCount_ == 1)
            changeTab(main);
    }

    void GroupProfile::closeClicked()
    {
        if (stackedWidget_->currentIndex() != main)
        {
            closeGallery();
            changeTab(main);
            return;
        }

        Utils::InterConnector::instance().setSidebarVisible(false);
    }

    void GroupProfile::editButtonClicked()
    {
        if (!chatInfo_)
            return;

        QString name = chatInfo_->Name_;
        QString desription = chatInfo_->About_;
        QString rules = chatInfo_->Rules_;

        auto avatar = editGroup(currentAimId_, name, desription, rules);
        if (name != chatInfo_->Name_)
        {
            Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
            collection.set_value_as_qstring("aimid", currentAimId_);
            collection.set_value_as_qstring("name", name);
            Ui::GetDispatcher()->post_message_to_core("chats/mod/name", collection.get());
        }

        if (desription != chatInfo_->About_)
        {
            Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
            collection.set_value_as_qstring("aimid", currentAimId_);
            collection.set_value_as_qstring("about", desription);
            Ui::GetDispatcher()->post_message_to_core("chats/mod/about", collection.get());
        }

        if (rules != chatInfo_->Rules_)
        {
            Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
            collection.set_value_as_qstring("aimid", currentAimId_);
            collection.set_value_as_qstring("rules", rules);
            Ui::GetDispatcher()->post_message_to_core("chats/mod/rules", collection.get());
        }

        if (!avatar.isNull())
        {
            const auto byteArray = avatarToByteArray(avatar);

            Ui::gui_coll_helper helper(GetDispatcher()->create_collection(), true);

            core::ifptr<core::istream> data_stream(helper->create_stream());
            if (!byteArray.isEmpty())
                data_stream->write((const uint8_t*)byteArray.data(), (uint32_t)byteArray.size());

            helper.set_value_as_stream("avatar", data_stream.get());
            helper.set_value_as_qstring("aimid", currentAimId_);

            GetDispatcher()->post_message_to_core("set_avatar", helper.get());
        }
    }

    void GroupProfile::approveAllClicked()
    {
        //!! todo: use new method for approve all
        const auto& members = listChatModel_->getMembers();
        Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
        collection.set_value_as_qstring("aimid", currentAimId_);

        core::ifptr<core::iarray> contacts_array(collection->create_array());
        contacts_array->reserve(members.size());

        for (const auto& iter : members)
            contacts_array->push_back(collection.create_qstring_value(iter.AimId_).get());

        collection.set_value_as_array("contacts", contacts_array.get());
        collection.set_value_as_bool("approve", true);
        Ui::GetDispatcher()->post_message_to_core("chats/pending/resolve", collection.get());

        closeGallery();
        changeTab(main);
    }

    void GroupProfile::memberMenu(QAction* action)
    {
        const auto params = action->data().toMap();
        const auto command = params[qsl("command")].toString();
        const auto aimId = params[qsl("aimid")].toString();

        if (command == ql1s("profile"))
        {
            Utils::InterConnector::instance().showSidebar(aimId);
        }
        else if (command == ql1s("spam"))
        {
            const auto friendly = Logic::GetFriendlyContainer()->getFriendly(aimId);
            if (Ui::ReportContact(aimId, friendly))
            {
                Logic::getContactListModel()->removeContactFromCL(aimId);
                GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::spam_contact, { { "From", "MembersList_Menu" } });
            }
        }
        else if (command == ql1s("block"))
        {
            blockUser(aimId, ActionType::POSITIVE);
        }
        else if (command == ql1s("readonly"))
        {
            readOnly(aimId, ActionType::POSITIVE);
        }
        else if (command == ql1s("revoke_readonly"))
        {
            readOnly(aimId, ActionType::NEGATIVE);
        }
        else if (command == ql1s("make_admin"))
        {
            changeRole(aimId, ActionType::POSITIVE);
        }
        else if (command == ql1s("revoke_admin"))
        {
            changeRole(aimId, ActionType::NEGATIVE);
        }
        else if (command == ql1s("leave"))
        {
            leaveClicked();
        }
    }

    void GroupProfile::menuAction(QAction* action)
    {
        const auto params = action->data().toMap();
        const auto command = params[qsl("command")].toString();

        if (command == ql1s("share"))
        {
            share();
        }
        else if (command == ql1s("copy_link"))
        {
            linkCopy();
        }
        else if (command == ql1s("copy"))
        {
            const auto text = params[qsl("text")].toString();
            copy(text);
            Utils::showToastOverMainWindow(QT_TRANSLATE_NOOP("toast", "Copied to clipboard"), Utils::scale_value(TOAST_OFFSET));
        }
    }

    void GroupProfile::memberActionResult(int)
    {
        if (!isActive())
            return;

        if (stackedWidget_->currentIndex() == list)
        {
            switch (currentListPage_)
            {
            case all:
                listChatModel_->loadAllMembers();
                break;

            case pending:
                listChatModel_->loadPending();
                break;

            case blocked:
                listChatModel_->loadBlocked();
                break;
            }
        }
        else
        {
            forceMembersRefresh_ = true;
            loadChatInfo();
        }
    }

    void GroupProfile::chatEvents(const QString& _aimid)
    {
        if (!isActive() || currentAimId_ != _aimid)
            return;

        memberActionResult(0);
    }

    void GroupProfile::onContactChanged(const QString& _aimid)
    {
        if (!isActive() || currentAimId_ != _aimid)
            return;

        updateMuteState();
    }

    void GroupProfile::updateMuteState()
    {
        QSignalBlocker b(notifications_);
        notifications_->setChecked(!Logic::getContactListModel()->isMuted(currentAimId_));
    }

    void GroupProfile::avatarClicked()
    {
        GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::profilescr_avatar_click, { { "chat_type", Utils::chatTypeByAimId(currentAimId_) } });
        Utils::InterConnector::instance().getMainWindow()->openAvatar(currentAimId_);
    }

    void GroupProfile::titleArrowClicked()
    {
        galleryPopup_->move(mapToGlobal(QPoint(Utils::scale_value(POPUP_HOR_OFFSET), Utils::scale_value(POPUP_VER_OFFSET))));
        galleryPopup_->show();
    }

    void GroupProfile::applyChatSettings()
    {
        if (!chatInfo_)
            return;

        bool somethingChanged = false;

        Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
        collection.set_value_as_qstring("aimid", currentAimId_);
        if (public_->isChecked() != chatInfo_->Public_)
        {
            collection.set_value_as_bool("public", public_->isChecked());
            somethingChanged = true;
        }

        if (chatInfo_->Stamp_ != editGroupLinkWidget_->getNick() && !invalidNick_)
        {
            auto nick = editGroupLinkWidget_->getNick();
            collection.set_value_as_qstring("stamp", nick);
            somethingChanged = true;

            aboutLinkToChat_->setText(Utils::getDomainUrl() % ql1c('/') % nick);

            GetDispatcher()->post_stats_to_core(public_->isChecked() ? core::stats::stats_event_names::profilescr_groupname_action : core::stats::stats_event_names::profilescr_newgroupurl_action, { { "chat_type", Utils::chatTypeByAimId(currentAimId_) } });
        }

        if (joinModeration_->isChecked() != chatInfo_->ApprovedJoin_)
        {
            collection.set_value_as_bool("approved", joinModeration_->isChecked());
            somethingChanged = true;
        }

        if (somethingChanged)
            Ui::GetDispatcher()->post_message_to_core("chats/mod/params", collection.get());

        Utils::InterConnector::instance().setFocusOnInput();

        editGroupLinkWidget_->clearHint();
    }

    void GroupProfile::checkApplyButton()
    {
        if (!chatInfo_)
            return;

        editGroupLinkWidget_->setVisible(public_->isChecked());
        aboutLinkToChatBlock_->setVisible(!public_->isChecked());

        if (invalidNick_)
        {
            aboutLinkToChat_->setTextLinkColor(Styling::getParameters().getColor(Styling::StyleVariable::TEXT_PRIMARY));
            aboutLinkToChat_->text_->setCursorForText();
            applyChatSettings_->setEnabled(false);
            return;
        }

        if (public_->isChecked() != chatInfo_->Public_)
        {
            aboutLinkToChat_->setTextLinkColor(Styling::getParameters().getColor(Styling::StyleVariable::TEXT_SOLID));
            aboutLinkToChat_->text_->unsetCursorForText();
            applyChatSettings_->setEnabled(true);
            return;
        }

        if (chatInfo_->Stamp_ != editGroupLinkWidget_->getNick())
        {
            aboutLinkToChat_->setTextLinkColor(Styling::getParameters().getColor(Styling::StyleVariable::TEXT_SOLID));
            aboutLinkToChat_->text_->unsetCursorForText();
            applyChatSettings_->setEnabled(true);
            return;
        }

        if (joinModeration_->isChecked() != chatInfo_->ApprovedJoin_)
        {
            aboutLinkToChat_->setTextLinkColor(Styling::getParameters().getColor(Styling::StyleVariable::TEXT_SOLID));
            aboutLinkToChat_->text_->unsetCursorForText();
            applyChatSettings_->setEnabled(true);
            return;
        }

        aboutLinkToChat_->setTextLinkColor(Styling::getParameters().getColor(Styling::StyleVariable::TEXT_PRIMARY));
        aboutLinkToChat_->text_->setCursorForText();
        applyChatSettings_->setEnabled(false);
    }

    void GroupProfile::publicClicked()
    {
        if (!chatInfo_)
            return;

        if (!public_->isChecked())
        {
            if (chatInfo_->Public_)
            {
                const QString text = QT_TRANSLATE_NOOP("popup_window", "If you change group type to a private link to the group will be changed, and the old one will stop working");
                auto confirm = Utils::GetConfirmationWithTwoButtons(
                    QT_TRANSLATE_NOOP("popup_window", "Cancel"),
                    QT_TRANSLATE_NOOP("popup_window", "Make private"),
                    text,
                    QT_TRANSLATE_NOOP("popup_window", "Public link will stop working"),
                    nullptr);

                if (!confirm)
                {
                    QSignalBlocker sb(public_);
                    public_->setChecked(true);
                    return;
                }

                Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
                collection.set_value_as_qstring("sn", currentAimId_);
                collection.set_value_as_bool("public", false);
                Ui::GetDispatcher()->post_message_to_core("suggest_group_nick", collection.get());
            }
            else
            {
                aboutLinkToChat_->setText(Utils::getDomainUrl() % ql1c('/') % chatInfo_->Stamp_);
                editGroupLinkWidget_->setNick(chatInfo_->Stamp_);
            }
        }

        if (public_->isChecked())
        {
            if (chatInfo_ && chatInfo_->Public_)
            {
                aboutLinkToChat_->setText(Utils::getDomainUrl() % ql1c('/') % chatInfo_->Stamp_);
                editGroupLinkWidget_->setNick(chatInfo_->Stamp_);
            }
            else
            {
                editGroupLinkWidget_->setNick(QString());
                Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
                collection.set_value_as_qstring("sn", currentAimId_);
                collection.set_value_as_bool("public", true);
                Ui::GetDispatcher()->post_message_to_core("suggest_group_nick", collection.get());
            }
        }

        checkApplyButton();
    }
}
