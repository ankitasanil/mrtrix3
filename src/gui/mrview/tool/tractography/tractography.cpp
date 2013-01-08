/*
   Copyright 2009 Brain Research Institute, Melbourne, Australia

   Written by J-Donald Tournier, 13/11/09.

   This file is part of MRtrix.

   MRtrix is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   MRtrix is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with MRtrix.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <QLabel>
#include <QListView>
#include <QStringListModel>

#include "mrtrix.h"
#include "gui/mrview/window.h"
#include "gui/mrview/tool/tractography/tractography.h"
#include "gui/mrview/tractogram.h"
#include "gui/dialog/file.h"
#include "gui/mrview/adjust_button.h"

namespace MR
{
  namespace GUI
  {
    namespace MRView
    {
      namespace Tool
      {


        class Tractography::Model : public QAbstractItemModel
        {
          public:
            Model (QObject* parent) :
              QAbstractItemModel (parent) { }

            QVariant data (const QModelIndex& index, int role) const {
              if (!index.isValid()) return QVariant();
              if (role == Qt::CheckStateRole) {
                return shown[index.row()] ? Qt::Checked : Qt::Unchecked;
              }
              if (role != Qt::DisplayRole) return QVariant();
              return shorten (tractograms[index.row()]->get_filename(), 20, 0).c_str();
            }
            bool setData (const QModelIndex& index, const QVariant& value, int role) {
              if (role == Qt::CheckStateRole) {
                shown[index.row()] =  (value == Qt::Checked);
                emit dataChanged(index, index);
                return true;
              }
              return QAbstractItemModel::setData (index, value, role);
            }

            Qt::ItemFlags flags (const QModelIndex& index) const {
              if (!index.isValid()) return 0;
              return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
            }
            QModelIndex index (int row, int column, const QModelIndex& parent = QModelIndex()) const { return createIndex (row, column); }
            QModelIndex parent (const QModelIndex& index) const { return QModelIndex(); }

            int rowCount (const QModelIndex& parent = QModelIndex()) const { return tractograms.size(); }
            int columnCount (const QModelIndex& parent = QModelIndex()) const { return 1; }

            void add_tractograms (std::vector<std::string>& filenames);
            void remove_tractogram (QModelIndex& index);
            VecPtr<Tractogram> tractograms;
            std::vector<bool> shown;
        };


        void Tractography::Model::add_tractograms (std::vector<std::string>& list)
        {
          beginInsertRows (QModelIndex(), tractograms.size(), tractograms.size() + list.size());
          for (size_t i = 0; i < list.size(); ++i)
            tractograms.push_back (new Tractogram (list[i]));
          shown.resize (tractograms.size(), true);
          endInsertRows();
        }

        void Tractography::Model::remove_tractogram (QModelIndex& index)
        {
          beginRemoveRows (QModelIndex(), index.row(), index.row());
          tractograms.erase (tractograms.begin() + index.row());
          shown.resize (tractograms.size(), true);
          endRemoveRows();
        }




        Tractography::Tractography (Window& main_window, Dock* parent) :
          Base (main_window, parent) { 
            QVBoxLayout* main_box = new QVBoxLayout (this);
            QHBoxLayout* layout = new QHBoxLayout;
            layout->setContentsMargins (0, 0, 0, 0);
            layout->setSpacing (0);

            QPushButton* button = new QPushButton (this);
            button->setToolTip (tr ("Open Tracks"));
            button->setIcon (QIcon (":/open.svg"));
            connect (button, SIGNAL (clicked()), this, SLOT (tractogram_open_slot ()));
            layout->addWidget (button, 1);

            button = new QPushButton (this);
            button->setToolTip (tr ("Close Tracks"));
            button->setIcon (QIcon (":/close.svg"));
            connect (button, SIGNAL (clicked()), this, SLOT (tractogram_close_slot ()));
            layout->addWidget (button, 1);

            main_box->addLayout (layout, 0);

            tractogram_list_view = new QListView (this);
            tractogram_list_view->setSelectionMode (QAbstractItemView::MultiSelection);
            tractogram_list_view->setDragEnabled (true);
            tractogram_list_view->viewport()->setAcceptDrops (true);
            tractogram_list_view->setDropIndicatorShown (true);

            tractogram_list_model = new Model (this);
            tractogram_list_view->setModel (tractogram_list_model);

            main_box->addWidget (tractogram_list_view, 1);

            QGridLayout* default_opt_grid = new QGridLayout;

            QWidget::setStyleSheet("QSlider { margin: 5 0 5 0px;  }"
                                   "QGroupBox { padding:7 3 0 0px; margin: 10 0 5 0px; border: 1px solid gray; border-radius: 4px}"
                                   "QGroupBox::title { subcontrol-position: top left; top:-8px; left:5px}");

            QGroupBox* slab_group_box = new QGroupBox (tr("crop to slab"));
            slab_group_box->setCheckable (true);
            slab_group_box->setChecked (true);
            default_opt_grid->addWidget (slab_group_box, 0, 0, 1, 2);

            QGridLayout* slab_layout = new QGridLayout;
            slab_group_box->setLayout(slab_layout);
            slab_layout->addWidget (new QLabel ("thickness (mm)"), 0, 0);
            AdjustButton* slab_entry = new AdjustButton (this, 0.1);
            slab_entry->setValue (5.0);
            slab_entry->setMin(0.0);
            connect (slab_entry, SIGNAL (valueChanged()), this, SLOT (on_slab_thickness_change()));
            slab_layout->addWidget (slab_entry, 0, 1);

            QSlider* slider;
            slider = new QSlider (Qt::Horizontal);
            slider->setRange (0,100);
            slider->setSliderPosition (int (100));
            connect (slider, SIGNAL (valueChanged (int)), this, SLOT (opacity_slot (int)));
            default_opt_grid->addWidget (new QLabel ("opacity"), 1, 0);
            default_opt_grid->addWidget (slider, 1, 1);

            slider = new QSlider (Qt::Horizontal);
            slider->setRange (0,100);
            slider->setSliderPosition (int (100));
            connect (slider, SIGNAL (valueChanged (int)), this, SLOT (line_thickness_slot (int)));
            default_opt_grid->addWidget (new QLabel ("line thickness"), 2, 0);
            default_opt_grid->addWidget (slider, 2, 1);

            main_box->addLayout (default_opt_grid, 0);
        }


        Tractography::~Tractography () {
          delete tractogram_list_model;
          delete tractogram_list_view;
        }

        void Tractography::tractogram_open_slot ()
        {
          Dialog::File dialog (this, "Select tractograms to open", true, false);
          if (dialog.exec()) {
            std::vector<std::string> list;
            dialog.get_selection (list);
            tractogram_list_model->add_tractograms (list);
          }
        }


        void Tractography::tractogram_close_slot ()
        {
          QModelIndexList indexes = tractogram_list_view->selectionModel()->selectedIndexes();
          while (indexes.size()) {
            tractogram_list_model->remove_tractogram (indexes.first());
            indexes = tractogram_list_view->selectionModel()->selectedIndexes();
          }
        }

        void Tractography::opacity_slot (int opacity) {
          CONSOLE(str(opacity));
        }

        void Tractography::line_thickness_slot (int thickness) {
          CONSOLE(str(thickness));
        }

        void Tractography::on_slab_thickness_change() {
        }

      }
    }
  }
}





