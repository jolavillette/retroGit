/*******************************************************************************
 * gui/MainWidget.cpp                                                          *
 *                                                                             *
 * Copyright (C) 2020 RetroShare Team <retroshare.project@gmail.com>           *
 *                                                                             *
 * This program is free software: you can redistribute it and/or modify        *
 * it under the terms of the GNU Affero General Public License as              *
 * published by the Free Software Foundation, either version 3 of the          *
 * License, or (at your option) any later version.                             *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                *
 * GNU Affero General Public License for more details.                         *
 *                                                                             *
 * You should have received a copy of the GNU Affero General Public License    *
 * along with this program. If not, see <https://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/

#include "MainWidget.h"
#include "GitGroupDialog.h"
#include "ui_MainWidget.h"

#include "gui/gxs/GxsIdDetails.h"
#include "gui/settings/rsharesettings.h"
#include "interface/rsGit.h"
#include "retroshare/rsgxsflags.h"
#include "retroshare/rsservicecontrol.h"
#include "services/p3Git.h"
#include "services/rsGitItems.h"

#include <QMenu>
#include <QTime>
#include <iostream>
#include <string>

#include "gui/common/RSTreeWidget.h"
#include "util/DateTime.h"
#include "util/qtthreadsutils.h"
#include <util/rsthreads.h>

#define IMAGE_GIT ":/images/git.png"

MainWidget::MainWidget(QWidget *parent, RetroGitNotify *notify):
    MainPage(parent),
    // mNotify(notify),
    ui(new Ui::MainWidget)
{
    (void)notify;
    ui->setupUi(this);

    /* Set initial size the splitter */
    ui->splitter->setStretchFactor(0, 0);
    ui->splitter->setStretchFactor(1, 1);

    /* Setup Combo Box strings */
    ui->comboBox->addItem(tr("All Repositories"));
    ui->comboBox->addItem(tr("My Repositories"));
    ui->comboBox->addItem(tr("Subscribed Repositories"));
    ui->comboBox->addItem(tr("Popular Repositories"));
    ui->comboBox->addItem(tr("Other Repositories"));
    connect(ui->comboBox, SIGNAL(currentIndexChanged(int)), this,
          SLOT(selectGroupSet(int)));
    connect(ui->treeWidget, SIGNAL(treeCustomContextMenuRequested(QPoint)), this,
          SLOT(groupListCustomPopupMenu(QPoint)));

    /* Setup Group Tree */
    mActiveGroupsItem =
      ui->treeWidget->addCategoryItem(tr("Repositories"), QIcon(), true);

    /* Add the New Group button */
    QToolButton *newGroupButton = new QToolButton(this);
    newGroupButton->setIcon(QIcon(":/icons/png/add.png"));
    newGroupButton->setToolTip(tr("Create Group"));
    ui->treeWidget->addToolButton(newGroupButton);

    // load settings
    processSettings(true);

    connect(newGroupButton, SIGNAL(clicked()), this, SLOT(createGroup()));

    loadGroupMeta();

    /* Register for Git events using the static RsEventType::GIT */
    if (rsEvents) {
    rsEvents->registerEventsHandler(
        [this](std::shared_ptr<const RsEvent> event) {
          RsQThreadUtils::postToObject(
              [=]() { handleEvent_main_thread(event); }, this);
        },
        mEventHandlerId, RsEventType::GIT);
    }
}

MainWidget::~MainWidget()
{
    if (rsEvents)
        rsEvents->unregisterEventsHandler(mEventHandlerId);

    delete ui;

    // save settings
    processSettings(false);
}

void MainWidget::showEvent(QShowEvent *event)
{
    MainPage::showEvent(event);

    // Load data once when the dialog is first shown
    if (!mInitialLoadDone)
    {
        mInitialLoadDone = true;
        updateDisplay();
    }
}

void MainWidget::processSettings(bool load)
{
  Settings->beginGroup("RetrGit");

  if (load) {
    // load settings

    // state of splitter
    ui->splitter->restoreState(Settings->value("SplitterList").toByteArray());

  } else {
    // save settings

    // state of splitter
    Settings->setValue("SplitterList", ui->splitter->saveState());
  }

  Settings->endGroup();
}

void MainWidget::createGroup()
{
    GitGroupDialog gitCreate(this);
    gitCreate.exec();
}

void MainWidget::loadGroupMeta()
{
  RsThread::async([this]() {
    // Fetch group metadata from backend
    std::list<RsGxsGroupId> groupIds; // empty list = get all groups
    std::vector<RsGitGroup> groups;

    if (!rsGit->getGroups(groupIds, groups)) {
      std::cerr << "MainWidget::loadGroupMeta() Error getting groups"
                << std::endl;
      return;
    }

    // Convert to RsGroupMetaData for display
    std::list<RsGroupMetaData> groupMeta;
    for (auto &group : groups) {
      groupMeta.push_back(group.mMeta);
    }

    // Update UI in main thread
    RsQThreadUtils::postToObject(
        [this, groupMeta]() {
          if (groupMeta.size() > 0) {
            insertGroupsData(groupMeta);
          }
        },
        this);
  });
}

void MainWidget::insertGroupsData(const std::list<RsGroupMetaData> &gitList)
{
  std::list<RsGroupMetaData>::const_iterator it;

  QList<GroupItemInfo> activeList;
  std::multimap<uint32_t, GroupItemInfo> popMap;
  QList<GroupItemInfo> popList;
  QList<GroupItemInfo> otherList;

  for (it = gitList.begin(); it != gitList.end(); ++it) {
    uint32_t flags = it->mSubscribeFlags;

    GroupItemInfo groupItemInfo;
    GroupMetaDataToGroupItemInfo(*it, groupItemInfo);

    bool add = false;
    if (mGroupSet == 0)
      add = true; // All

    if (IS_GROUP_ADMIN(flags)) {
      if (mGroupSet == 1 || mGroupSet == 0)
        add = true;
    } else if (IS_GROUP_SUBSCRIBED(flags)) {
      if (mGroupSet == 2 || mGroupSet == 0)
        add = true;
    } else {
      popMap.insert(std::make_pair(it->mPop, groupItemInfo));
    }

    if (add && (IS_GROUP_ADMIN(flags) || IS_GROUP_SUBSCRIBED(flags))) {
      activeList.push_back(groupItemInfo);
    }
  }

  uint32_t popCount = 5;
  if (popCount < popMap.size() / 10) {
    popCount = popMap.size() / 10;
  }

  uint32_t i = 0;
  uint32_t popLimit = 0;
  std::multimap<uint32_t, GroupItemInfo>::reverse_iterator rit;
  for (rit = popMap.rbegin(); ((rit != popMap.rend()) && (i < popCount));
       ++rit, i++)
    ;
  if (rit != popMap.rend()) {
    popLimit = rit->first;
  }

  for (rit = popMap.rbegin(); rit != popMap.rend(); ++rit) {
    if (rit->second.popularity < (int)popLimit) {
      otherList.append(rit->second);
      if (mGroupSet == 4 || mGroupSet == 0)
        activeList.append(rit->second);
    } else {
      popList.append(rit->second);
      if (mGroupSet == 3 || mGroupSet == 0)
        activeList.append(rit->second);
    }
  }

  ui->treeWidget->fillGroupItems(mActiveGroupsItem, activeList);
}

void MainWidget::selectGroupSet(int index)
{
  mGroupSet = index;
  updateDisplay();
}

void MainWidget::GroupMetaDataToGroupItemInfo(const RsGroupMetaData &groupInfo,GroupItemInfo &groupItemInfo)
{
  groupItemInfo.id = QString::fromStdString(groupInfo.mGroupId.toStdString());
  groupItemInfo.name = QString::fromUtf8(groupInfo.mGroupName.c_str());
  //groupItemInfo.description = QString(); // description not in RsGroupMetaData
  groupItemInfo.popularity = groupInfo.mPop;
  groupItemInfo.lastpost = DateTime::DateTimeFromTime_t(groupInfo.mLastPost);
  groupItemInfo.subscribeFlags = groupInfo.mSubscribeFlags;

  groupItemInfo.icon = GxsIdDetails::makeColoredGroupIcon(groupInfo.mGroupId, IMAGE_GIT, GxsIdDetails::ORIGINAL);
}

void MainWidget::updateDisplay()
{
    // Load all group metadata
    loadGroupMeta();
}

void MainWidget::handleEvent_main_thread(std::shared_ptr<const RsEvent> event)
{
    const RsGitEvent *e = dynamic_cast<const RsGitEvent *>(event.get());

    if (!e)
    return;

    switch (e->mGitEventCode) {
        case RsGitEventCode::NEW_GIT:
            updateDisplay(); // Refresh global list
            break;
        case RsGitEventCode::GIT_UPDATED:
            updateDisplay(); // Refresh global list
            break;
        case RsGitEventCode::SUBSCRIBE_STATUS_CHANGED:
            updateDisplay();
            break;

    default:
        break;
    }
}

void MainWidget::groupListCustomPopupMenu(QPoint /*point*/)
{
    if (!ui->treeWidget || !ui->treeWidget->treeWidget())
        return;
    QTreeWidgetItem *item = ui->treeWidget->treeWidget()->currentItem();
    if (!item)
        return;

    QString groupId = ui->treeWidget->itemId(item);
    if (groupId.isEmpty())
        return;

    int subscribeFlags = ui->treeWidget->subscribeFlags(groupId);

    QMenu contextMnu(this);
    QAction *action;

    action = contextMnu.addAction(QIcon(":/images/info.png"), tr("Show Group Details"), this, SLOT(showGroupDetails()));
    action->setEnabled(!groupId.isEmpty());

    action = contextMnu.addAction(QIcon(":/images/edit.png"), tr("Edit Group Details"), this, SLOT(editGroupDetails()));
    action->setEnabled(!groupId.isEmpty() && IS_GROUP_ADMIN(subscribeFlags));

    contextMnu.exec(QCursor::pos());
}

void MainWidget::showGroupDetails()
{
    if (!ui->treeWidget || !ui->treeWidget->treeWidget())
        return;
    QTreeWidgetItem *item = ui->treeWidget->treeWidget()->currentItem();
    if (!item)
        return;

    QString groupId = ui->treeWidget->itemId(item);
    if (groupId.isEmpty())
        return;

    GitGroupDialog dialog(GxsGroupDialog::MODE_SHOW,RsGxsGroupId(groupId.toStdString()), this);
    dialog.exec();
}

void MainWidget::editGroupDetails()
{
    if (!ui->treeWidget || !ui->treeWidget->treeWidget())
        return;
    QTreeWidgetItem *item = ui->treeWidget->treeWidget()->currentItem();
    if (!item)
    return;

    QString groupId = ui->treeWidget->itemId(item);
    if (groupId.isEmpty())
    return;

    GitGroupDialog dialog(GxsGroupDialog::MODE_EDIT,RsGxsGroupId(groupId.toStdString()), this);
    dialog.exec();
}
