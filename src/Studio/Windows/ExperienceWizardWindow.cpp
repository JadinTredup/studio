/****************************************************************************
**
** Copyright 2019 neuromore co
** Contact: https://neuromore.com/contact
**
** Commercial License Usage
** Licensees holding valid commercial neuromore licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and neuromore. For licensing terms
** and conditions see https://neuromore.com/licensing. For further
** information use the contact form at https://neuromore.com/contact.
**
** neuromore Public License Usage
** Alternatively, this file may be used under the terms of the neuromore
** Public License version 1 as published by neuromore co with exceptions as 
** appearing in the file neuromore-class-exception.md included in the 
** packaging of this file. Please review the following information to 
** ensure the neuromore Public License requirements will be met: 
** https://neuromore.com/npl
**
****************************************************************************/

// include the required headers
#include "ExperienceWizardWindow.h"

#include <Core/LogManager.h>
#include <QtBaseManager.h>
#include <Backend/BackendHelpers.h>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QIcon>
#include <Graph/ChannelSelectorNode.h>
#include <Backend/FileHierarchyGetRequest.h>
#include <Backend/FileHierarchyGetResponse.h>
#include <Backend/FilesGetRequest.h>
#include <Backend/FilesGetResponse.h>

using namespace Core;

// constructor
ExperienceWizardWindow::ExperienceWizardWindow(const User& user, QWidget* parent) :
   QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint),
   mUser(user),
   mMainLayout(),
   mUserLayout(),
   mUserDesc("User:"),
   mUserLabel(user.CreateFullName().AsChar()),
   mClassifierLayout(),
   mClassifierSelectDesc("Classifier:"),
   mClassifierSelect(),
   mStateMachineLayout(),
   mStateMachineSelectDesc("State Machine:"),
   mStateMachineSelect(),
   mTableWidget(),
   mHeaderType("Type"),
   mHeaderName("Name"),
   mHeaderEdit("Edit"),
   mCreateButton("Create")
{
   // set the window title
   setWindowTitle("Experience Wizard");
   setWindowIcon(GetQtBaseManager()->FindIcon("Images/Icons/Users.png"));
   setMinimumWidth(600);
   setMinimumHeight(400);
   setModal(true);
   setWindowModality(Qt::ApplicationModal);

   // add the main vertical layout
   setLayout(&mMainLayout);

   /////////////////////////////////////////////////
   // user

   mUserDesc.setMinimumWidth(100);
   mUserLabel.setMinimumWidth(200);
   mUserLayout.setSpacing(6);
   mUserLayout.setAlignment(Qt::AlignCenter);
   mUserLayout.addWidget(&mUserDesc);
   mUserLayout.addWidget(&mUserLabel);
   mMainLayout.addLayout(&mUserLayout);

   /////////////////////////////////////////////////
   // classifier

   mClassifierSelectDesc.setMinimumWidth(100);
   mClassifierSelect.setMinimumWidth(200);
   mClassifierLayout.setSpacing(6);
   mClassifierLayout.setAlignment(Qt::AlignCenter);
   mClassifierLayout.addWidget(&mClassifierSelectDesc);
   mClassifierLayout.addWidget(&mClassifierSelect);
   mMainLayout.addLayout(&mClassifierLayout);

   connect(&mClassifierSelect, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ExperienceWizardWindow::OnClassifierSelectIndexChanged);

   /////////////////////////////////////////////////
   // state machine

   mStateMachineSelectDesc.setMinimumWidth(100);
   mStateMachineSelect.setMinimumWidth(200);
   mStateMachineLayout.setSpacing(6);
   mStateMachineLayout.setAlignment(Qt::AlignCenter);
   mStateMachineLayout.addWidget(&mStateMachineSelectDesc);
   mStateMachineLayout.addWidget(&mStateMachineSelect);
   mMainLayout.addLayout(&mStateMachineLayout);

   connect(&mStateMachineSelect, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ExperienceWizardWindow::OnStateMachineSelectIndexChanged);

   /////////////////////////////////////////////////
   // table

   mMainLayout.addWidget(&mTableWidget);
   mTableWidget.setEnabled(true);

   // columns
   mTableWidget.setColumnCount(3);
   mTableWidget.setColumnWidth(0, 80);
   mTableWidget.setColumnWidth(1, 120);

   // header
   mHeaderType.setTextAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
   mHeaderName.setTextAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
   mHeaderEdit.setTextAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
   mTableWidget.setHorizontalHeaderItem(0, &mHeaderType);
   mTableWidget.setHorizontalHeaderItem(1, &mHeaderName);
   mTableWidget.setHorizontalHeaderItem(2, &mHeaderEdit);

   // tweaks
   //mTableWidget.horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Stretch);
   mTableWidget.horizontalHeader()->setStretchLastSection(true);
   mTableWidget.horizontalHeader()->show();

   // don't show the vertical header
   mTableWidget.verticalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Fixed);
   mTableWidget.verticalHeader()->setDefaultSectionSize(128);

   mTableWidget.verticalHeader()->hide();

   // complete row selection
   mTableWidget.setSelectionBehavior(QAbstractItemView::SelectRows);
   mTableWidget.setSelectionMode(QAbstractItemView::SelectionMode::NoSelection);
   mTableWidget.setFocusPolicy(Qt::NoFocus);
   mTableWidget.setAlternatingRowColors(false);
   mTableWidget.setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

   /////////////////////////////////////////////////
   // TABLE ROWS DUMMY PART

   CreateChannelSelectorRow("Inhibit");
   CreateChannelSelectorRow("Augment");

   /////////////////////////////////////////////////
   // create button

   mCreateButton.setIcon(GetQtBaseManager()->FindIcon("Images/Icons/Plus.png"));
   mCreateButton.setToolTip("Create this protocol for selected user.");
   mMainLayout.addWidget(&mCreateButton);

   connect(&mCreateButton, &QPushButton::clicked, this, &ExperienceWizardWindow::OnCreateClicked);

   /////////////////////////////////////////////////
   // request backend file hierarchy

   RequestFileHierarchy();

   /////////////////////////////////////////////////
   // finish

   GetQtBaseManager()->CenterToScreen(this);
   show();
}


// destructor
ExperienceWizardWindow::~ExperienceWizardWindow()
{
}

void ExperienceWizardWindow::OnClassifierSelectIndexChanged(int index)
{
   if (index < 0 || index >= mClassifierSelect.count())
      return;

   // load classifier from backend
   FilesGetRequest request(GetUser()->GetToken(), GetClassifierId().toLocal8Bit().data());
   QNetworkReply* reply = GetBackendInterface()->GetNetworkAccessManager()->ProcessRequest(request, Request::UIMODE_SILENT);
   connect(reply, &QNetworkReply::finished, this, [reply, this]()
   {
      QNetworkReply* networkReply = qobject_cast<QNetworkReply*>(sender());

      FilesGetResponse response(networkReply);
      if (response.HasError() == true)
         return;

      // parse the json into classifier instance
      mGraphImporter.LoadFromString(response.GetJsonContent(), &mClassifier);

      // iterate nodes in this classifier
      const uint32 numNodes = mClassifier.GetNumNodes();
      for (uint32_t i = 0; i < numNodes; i++)
      {
         Node* n = mClassifier.GetNode(i);

         // WIP: ChannelSelectorNode
         if (n->GetType() == ChannelSelectorNode::TYPE_ID)
         {
            // iterate attributes
            const uint32 numAtt = n->GetNumAttributes();
            for (uint32_t j = 0; j < numAtt; j++)
            {
               Core::AttributeSettings* settings = n->GetAttributeSettings(j);
               Core::Attribute*         attrib   = n->GetAttributeValue(j);

               // look for the channels string array
               if (settings->GetInternalNameString() == "channels" && 
                   settings->GetInterfaceType() == ATTRIBUTE_INTERFACETYPE_STRINGARRAY)
               {
                  // DEBUG
                  Core::String val;
                  if (attrib->ConvertToString(val))
                     printf("%s: %s \n", settings->GetName(), val.AsChar());
               }
            }
         }

         // IMPLEMENT OTHER QUICK CONFIGURABLE NODE TYPES HERE
      }
   });
}

void ExperienceWizardWindow::OnStateMachineSelectIndexChanged(int index)
{
   if (index < 0 || index >= mStateMachineSelect.count())
      return;
}

void ExperienceWizardWindow::OnCreateClicked()
{
   //TODO
}

void ExperienceWizardWindow::RequestFileHierarchy()
{
   // clear and disable classifier combobox
   mClassifierSelect.setEnabled(false);
   mClassifierSelect.clear();
   mClassifierSelect.blockSignals(true);

   // clear and disable state machine combobox
   mStateMachineSelect.setEnabled(false);
   mStateMachineSelect.clear();
   mStateMachineSelect.blockSignals(true);

   FileHierarchyGetRequest request(GetUser()->GetToken(), GetUser()->GetIdString());
   QNetworkReply* reply = GetBackendInterface()->GetNetworkAccessManager()->ProcessRequest(request, Request::UIMODE_SILENT);
   connect(reply, &QNetworkReply::finished, this, [reply, this]()
   {
      QNetworkReply* networkReply = qobject_cast<QNetworkReply*>(sender());

      FileHierarchyGetResponse response(networkReply);
      if (response.HasError() == true)
         return;

      const Core::Json& json = response.GetJson();

      // walk file hierarchy
      Json::Item rootItem = json.GetRootItem();
      if (rootItem.IsNull() == false)
      {
         Json::Item dataItem = rootItem.Find("data");
         if (dataItem.IsNull() == false)
         {
            Json::Item rootFoldersItem = dataItem.Find("rootFolders");
            if (rootFoldersItem.IsArray() == true)
            {
               const uint32 numRootFolders = rootFoldersItem.Size();
               for (uint32 i = 0; i < numRootFolders; ++i)
                  ProcessFolder(rootFoldersItem[i]);
            }
         }
      }

      // sort and enable classifier combobox
      mClassifierSelect.model()->sort(0);
      mClassifierSelect.setEnabled(true);
      mClassifierSelect.blockSignals(false);
      mClassifierSelect.setCurrentIndex(0);

      // sort and enable statemachine combobox
      mStateMachineSelect.model()->sort(0);
      mStateMachineSelect.setEnabled(true);
      mStateMachineSelect.blockSignals(false);
      mStateMachineSelect.setCurrentIndex(0);
   });
}

void ExperienceWizardWindow::ProcessFolder(const Json::Item& folder)
{
   const Json::Item nameItem = folder.Find("name");
   const Json::Item folderIdItem = folder.Find("folderId");
   const Json::Item attributesItem = folder.Find("attributes");

   // iterate files in this folder
   Json::Item filesItem = folder.Find("files");
   if (filesItem.IsArray() == true)
   {
      const uint32 numFiles = filesItem.Size();
      for (uint32 i = 0; i < numFiles; ++i)
      {
         // find attributes
         Json::Item nameItem = filesItem[i].Find("name");
         Json::Item typeItem = filesItem[i].Find("type");
         Json::Item fileIdItem = filesItem[i].Find("fileId");
         Json::Item revisionItem = filesItem[i].Find("revision");

         // convert to QtStrings
         QString name(nameItem.GetString());
         QString type(typeItem.GetString());
         QString id(fileIdItem.GetString());

         // add classifiers to combobox
         if (type == "CLASSIFIER")
            mClassifierSelect.addItem(name, id);

         // add statemachines to combobox
         else if (type == "STATEMACHINE")
            mStateMachineSelect.addItem(name, id);
      }
   }

   // recursively add subfolders
   Json::Item foldersItem = folder.Find("folders");
   if (foldersItem.IsArray() == true)
   {
      const uint32 numFolders = foldersItem.Size();
      for (uint32 i = 0; i < numFolders; ++i)
         ProcessFolder(foldersItem[i]);
   }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CHANNEL SELECTOR
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ExperienceWizardWindow::CreateChannelSelectorRow(const char* name)
{
   const uint32 row = mTableWidget.rowCount();
   mTableWidget.insertRow(row);

   //////////////////////////////////////////////////////////////////////////////////
   // 1: TYPE/ICON

   QLabel* lblIcon = new QLabel();
   lblIcon->setPixmap(QPixmap(":/Images/Graph/" + QString(ChannelSelectorNode::Uuid()) + ".png"));
   lblIcon->setFixedSize(64, 64);
   lblIcon->setScaledContents(true);
   
   QHBoxLayout* lblLayout = new QHBoxLayout();
   lblLayout->setAlignment(Qt::AlignCenter);
   lblLayout->addWidget(lblIcon);
   
   QWidget* lblWidget = new QWidget();
   lblWidget->setLayout(lblLayout);
   
   //////////////////////////////////////////////////////////////////////////////////
   // 2: NAME

   QTableWidgetItem* secondItem = new QTableWidgetItem(name);
   secondItem->setTextAlignment(Qt::AlignCenter);
   secondItem->setFlags(secondItem->flags() ^ Qt::ItemIsEditable);

   //////////////////////////////////////////////////////////////////////////////////
   // 3: EDIT

   QWidget* container = new QWidget();
   QVBoxLayout* vl = new QVBoxLayout(container);

   QListWidget* list = new QListWidget();
   list->setSpacing(0);
   list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
   list->setSelectionMode(QAbstractItemView::SelectionMode::NoSelection);

   CreateChannelSelectorListItem(*list, "Fpz", "Alpha");
   CreateChannelSelectorListItem(*list, "F3", "Beta1");
   CreateChannelSelectorListItem(*list, "F4", "Beta2");

   QHBoxLayout* hlnew = new QHBoxLayout();
   hlnew->setContentsMargins(0, 0, 0, 0);

   QComboBox* qboxch = new QComboBox();
   qboxch->addItem("Fpz");
   qboxch->addItem("F3");
   qboxch->addItem("Fz");
   qboxch->addItem("F4");
   qboxch->addItem("C3");
   qboxch->addItem("Cz");
   qboxch->addItem("C4");
   qboxch->addItem("Pz");

   QComboBox* qboxband = new QComboBox();
   qboxband->addItem("Alpha");
   qboxband->addItem("Alpha/Theta");
   qboxband->addItem("Beta1");
   qboxband->addItem("Beta2");
   qboxband->addItem("Beta3");
   qboxband->addItem("Delta");
   qboxband->addItem("SMR");
   qboxband->addItem("Theta");
   qboxband->addItem("Theta/Beta");

   QPushButton* qbtnadd = new QPushButton();
   qbtnadd->setToolTip("Add this combination");
   qbtnadd->setIcon(GetQtBaseManager()->FindIcon("Images/Icons/Plus.png"));
   qbtnadd->setProperty("List", QVariant::fromValue((void*)list));
   qbtnadd->setProperty("Channel", QVariant::fromValue((void*)qboxch));
   qbtnadd->setProperty("Band", QVariant::fromValue((void*)qboxband));

   connect(qbtnadd, &QPushButton::clicked, this, &ExperienceWizardWindow::OnChannelSelectorListItemAdd);

   hlnew->addWidget(qboxch);
   hlnew->addWidget(qboxband);
   hlnew->addWidget(qbtnadd);

   vl->addWidget(list);
   vl->addLayout(hlnew);

   //////////////////////////////////////////////////////////////////////////////////

   mTableWidget.setCellWidget(row, 0, lblWidget);
   mTableWidget.setItem(      row, 1, secondItem);
   mTableWidget.setCellWidget(row, 2, container);
}

void ExperienceWizardWindow::CreateChannelSelectorListItem(QListWidget& list, const char* channel, const char* band)
{
   // avoid duplicates
   if (HasChannelSelectorListItem(list, channel, band))
      return;

   // create the list item and its internal widget/layout
   QListWidgetItem* item   = new QListWidgetItem();
   QWidget*         widget = new QWidget();
   QHBoxLayout*     layout = new QHBoxLayout(widget);

   // no margin
   layout->setContentsMargins(0, 0, 0, 0);

   // create the internal widgets
   QLabel*      lblChannel = new QLabel(channel);
   QLabel*      lblBand    = new QLabel(band);
   QPushButton* btnDelete  = new QPushButton();

   // set names for lookup
   lblChannel->setObjectName("Channel");
   lblBand->setObjectName("Band");
   btnDelete->setObjectName("Delete");

   // configure delete button
   btnDelete->setToolTip("Remove this combination");
   btnDelete->setIcon(GetQtBaseManager()->FindIcon("Images/Icons/Minus.png"));
   btnDelete->setProperty("ListWidgetItem", QVariant::fromValue((void*)item));

   // link delete button click event
   connect(btnDelete, &QPushButton::clicked, this, &ExperienceWizardWindow::OnChannelSelectorListItemDelete);

   // add everything to layout
   layout->addWidget(lblChannel);
   layout->addWidget(lblBand);
   layout->addWidget(btnDelete);
   layout->setSizeConstraint(QLayout::SizeConstraint::SetMinimumSize);

   // set item size from widget size
   item->setSizeHint(widget->sizeHint());

   // add it to the list
   list.addItem(item);
   list.setItemWidget(item, widget);
}

bool ExperienceWizardWindow::HasChannelSelectorListItem(QListWidget& list, const char* channel, const char* band)
{
   const uint32 numItems = list.count();
   for (uint32 i = 0; i < numItems; i++)
   {
      // find widgets
      QListWidgetItem* lwi = list.item(i);
      QWidget*         w   = list.itemWidget(lwi);
      QLabel*          c   = w->findChild<QLabel*>("Channel");
      QLabel*          b   = w->findChild<QLabel*>("Band");

      // match
      if (c && b && c->text() == channel && b->text() == band)
         return true;
   }
   return false;
}

void ExperienceWizardWindow::OnChannelSelectorListItemAdd()
{
   if (!sender())
      return;

   // get button, list and ch+band
   QPushButton* btn  = qobject_cast<QPushButton*>(sender());
   QListWidget* list = (QListWidget*)btn->property("List").value<void*>();
   QComboBox*   ch   = (QComboBox*)btn->property("Channel").value<void*>();
   QComboBox*   band = (QComboBox*)btn->property("Band").value<void*>();

   // create entry in list
   CreateChannelSelectorListItem(*list, ch->currentText().toLocal8Bit().data(), band->currentText().toLocal8Bit().data());

   // TODO: call some kind of refresh (=update node strings from ui)
}

void ExperienceWizardWindow::OnChannelSelectorListItemDelete()
{
   if (!sender())
      return;

   // get button and listitem
   QPushButton*     b = qobject_cast<QPushButton*>(sender());
   QListWidgetItem* w = (QListWidgetItem*)b->property("ListWidgetItem").value<void*>();

   // delete entry from list
   delete w;

   // TODO: call some kind of refresh (=update node strings from ui)
}
