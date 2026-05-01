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
#include "ui_MainWidget.h"
#include "GitGroupDialog.h"

#include "services/p3Git.h"
#include "interface/rsGit.h"
#include "services/rsGitItems.h"
#include "retroshare/rsservicecontrol.h"

#include <iostream>
#include <string>
#include <QTime>
#include <QMenu>

#include <util/rsthreads.h>
#include "util/qtthreadsutils.h"

MainWidget::MainWidget(QWidget *parent, RetroGitNotify *notify) :
	MainPage(parent),
	//mNotify(notify),
	ui(new Ui::MainWidget)
{
	(void)notify;
	ui->setupUi(this);
	
    /* Set initial size the splitter */
    ui->splitter->setStretchFactor(0, 0);
    ui->splitter->setStretchFactor(1, 1);
	
	connect( ui->toolButton_createGit, SIGNAL(clicked()), this, SLOT(createGroup()));

	loadGroupMeta();
}

MainWidget::~MainWidget()
{
	delete ui;
}

void MainWidget::createGroup()
{
	GitGroupDialog gitCreate(this);
	gitCreate.exec();
}

void MainWidget::loadGroupMeta()
{
	RsThread::async([this]()
	{
		// Fetch group metadata from backend
		std::list<RsGxsGroupId> groupIds; // empty list = get all groups
		std::vector<RsGitGroup> groups;
		
		if (!rsRetroGit->getGroups(groupIds, groups))
		{
			std::cerr << "MainWidget::loadGroupMeta() Error getting groups" << std::endl;
			return;
		}

		// Convert to RsGroupMetaData for display
		std::list<RsGroupMetaData> groupMeta;
		for (auto& group : groups)
		{
			groupMeta.push_back(group.mMeta);
		}

		// Update UI in main thread
		RsQThreadUtils::postToObject([this, groupMeta]()
		{
			if (groupMeta.size() > 0)
			{
				insertGroupsData(groupMeta);
			}
		}, this);
	});
}

void MainWidget::insertGroupsData(const std::list<RsGroupMetaData> &gitList)
{
    ui->treeWidget->clear();
    
    for (const auto& meta : gitList) {
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeWidget);
        item->setText(0, QString::fromUtf8(meta.mGroupName.c_str()));
        item->setData(0, Qt::UserRole, QString::fromStdString(meta.mGroupId.toStdString()));
    }
}


