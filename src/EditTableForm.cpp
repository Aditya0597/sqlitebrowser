#include "EditTableForm.h"
#include "ui_EditTableForm.h"
#include "EditFieldForm.h"
#include <QMessageBox>
#include <QPushButton>
#include "sqlitedb.h"

/*
 *  Constructs a editTableForm as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  true to construct a modal dialog.
 */
editTableForm::editTableForm(QWidget* parent, Qt::WindowFlags fl)
    : QDialog(parent, fl),
      modified(false),
      pdb(0),
      ui(new Ui::editTableForm)
{
    ui->setupUi(this);
}

/*
 *  Destroys the object and frees any allocated resources
 */
editTableForm::~editTableForm()
{
    delete ui;
}

void editTableForm::setActiveTable(DBBrowserDB * thedb, QString tableName)
{
    // Set variables
    pdb = thedb;
    curTable = tableName;

    // Editing an existing table?
    if(curTable != "")
    {
        // Existing table, so load and set the current layout
        populateFields();

        // And create a savepoint
        pdb->executeSQL(QString("SAVEPOINT edittable_%1_save;").arg(curTable));
    }

    // Update UI
    ui->editTableName->setText(curTable);
    checkInput();
}

void editTableForm::populateFields()
{
    if (!pdb) return;

    //make sure we are not using cached information
    pdb->updateSchema();

    fields= pdb->getTableFields(curTable);
    types= pdb->getTableTypes(curTable);
    ui->treeWidget->model()->removeRows(0, ui->treeWidget->model()->rowCount());
    QStringList::Iterator tt = types.begin();
    for ( QStringList::Iterator ct = fields.begin(); ct != fields.end(); ++ct ) {
        QTreeWidgetItem *fldItem = new QTreeWidgetItem();
        fldItem->setText( 0, *ct  );
        fldItem->setText( 1, *tt );
        ui->treeWidget->addTopLevelItem(fldItem);
        ++tt;
    }
}

void editTableForm::accept()
{
    // Are we editing an already existing table or designing a new one? In the first case there is a table name set,
    // in the latter the current table name is empty
    if(curTable == "")
    {
        // Creation of new table

        // Build SQL statement from what the use entered
        QString sql = QString("CREATE TABLE `%1` (").arg(ui->editTableName->text());
        for(int i=0;i<ui->treeWidget->topLevelItemCount();i++)
            sql.append(QString("%1 %2,").arg(ui->treeWidget->topLevelItem(i)->text(0)).arg(ui->treeWidget->topLevelItem(i)->text(1)));
        sql.remove(sql.count() - 1, 1);     // Remove last comma
        sql.append(");");

        // Execute it
        modified = true;
        if (!pdb->executeSQL(sql)){
            QString error("Error creating table. Message from database engine:\n");
            error.append(pdb->lastErrorMessage).append("\n\n").append(sql);
            QMessageBox::warning( this, QApplication::applicationName(), error );
            return;
        }
    } else {
        // Editing of old table

        // Rename table if necessary
        if(ui->editTableName->text() != curTable)
        {
            QApplication::setOverrideCursor( Qt::WaitCursor ); // this might take time
            modified = true;
            QString newName = ui->editTableName->text();
            QString sql = QString("ALTER TABLE `%1` RENAME TO `%2`").arg(curTable, newName);
            if (!pdb->executeSQL(sql)){
                QApplication::restoreOverrideCursor();
                QString error("Error renaming table. Message from database engine:\n");
                error.append(pdb->lastErrorMessage).append("\n\n").append(sql);
                QMessageBox::warning( this, QApplication::applicationName(), error );
                return;
            } else {
                QApplication::restoreOverrideCursor();
            }
        }

        // Release the savepoint
        pdb->executeSQL(QString("RELEASE SAVEPOINT edittable_%1_save;").arg(curTable));
    }

    QDialog::accept();
}

void editTableForm::reject()
{
    // Have we been in the process of editing an old table?
    if(curTable != "")
    {
        // Then rollback to our savepoint
         pdb->executeSQL(QString("ROLLBACK TO SAVEPOINT edittable_%1_save;").arg(curTable));
    }

    QDialog::reject();
}

void editTableForm::checkInput()
{
    ui->editTableName->setText(ui->editTableName->text().trimmed());
    bool valid = true;
    if(ui->editTableName->text().isEmpty() || ui->editTableName->text().contains(" "))
        valid = false;
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(valid);
}

void editTableForm::editField()
{
    if( !ui->treeWidget->currentItem()){
        return;
    }
    QTreeWidgetItem *item = ui->treeWidget->currentItem();
    editFieldForm * fieldForm = new editFieldForm( this );
    fieldForm->setModal(true);
    fieldForm->setInitialValues(pdb, curTable == "", curTable, item->text(0), item->text(1));
    if (fieldForm->exec())
    {
        modified = true;
        item->setText(0,fieldForm->field_name);
        item->setText(1,fieldForm->field_type);
    }
    //not until nested transaction are supported
    //if (!pdb->executeSQL(QString("BEGIN TRANSACTION;"))) goto rollback;

    //             QString sql = "CREATE TEMPORARY TABLE TEMP_TABLE(";
    //             Q3ListViewItemIterator it( fieldListView );
    //             Q3ListViewItem * item;
    //             while ( it.current() ) {
    //                 item = it.current();
    //                 sql.append(item->text(0));
    //                 sql.append(" ");
    //                 sql.append(item->text(1));
    //                 if (item->nextSibling() != 0)
    //                 {
    //                     sql.append(", ");
    //                 }
    //                 ++it;
    //             }
    //             sql.append(");");
    //             if (!pdb->executeSQL(sql)) goto rollback;
    //
    //             sql = "INSERT INTO TEMP_TABLE SELECT ";
    //             for ( QStringList::Iterator ct = fields.begin(); ct != fields.end(); ++ct )		    {
    //                 sql.append( *ct );
    //                 if (*ct != fields.last())
    //                 {
    //                     sql.append(", ");
    //                 }
    //             }
    //
    //             sql.append(" FROM ");
    //             sql.append(curTable);
    //             sql.append(";");
    //             if (!pdb->executeSQL(sql)) goto rollback;
    //
    //             sql = "DROP TABLE ";
    //             sql.append(curTable);
    //             sql.append(";");
    //             if (!pdb->executeSQL(sql)) goto rollback;
    //
    //             sql = "CREATE TABLE ";
    //             sql.append(curTable);
    //             sql.append(" (");
    //             it = Q3ListViewItemIterator( fieldListView );
    //             while ( it.current() ) {
    //                 item = it.current();
    //                 sql.append(item->text(0));
    //                 sql.append(" ");
    //                 sql.append(item->text(1));
    //                 if (item->nextSibling() != 0)
    //                 {
    //                     sql.append(", ");
    //                 }
    //                 ++it;
    //             }
    //             sql.append(");");
    //             if (!pdb->executeSQL(sql)) goto rollback;
    //
    //             sql = "INSERT INTO ";
    //             sql.append(curTable);
    //             sql.append(" SELECT ");
    //             it = Q3ListViewItemIterator( fieldListView );
    //             while ( it.current() ) {
    //                 item = it.current();
    //                 sql.append(item->text(0));
    //                 if (item->nextSibling() != 0)
    //                 {
    //                     sql.append(", ");
    //                 }
    //                 ++it;
    //             }
    //             sql.append(" FROM TEMP_TABLE;");
    //             if (!pdb->executeSQL(sql)) goto rollback;
    //
    //             if (!pdb->executeSQL(QString("DROP TABLE TEMP_TABLE;"))) goto rollback;
    //             //not until nested transaction are supported
    //             //if (!pdb->executeSQL(QString("COMMIT;"))) goto rollback;
    //
    //             setActiveTable(pdb, curTable);
    //        }
    //        //everything ok, just return
    //        QApplication::restoreOverrideCursor();  // restore original cursor
    //        return;
    //
    //        rollback:
    //            QApplication::restoreOverrideCursor();  // restore original cursor
    //            QString error = "Error editing field. Message from database engine:  ";
    //            error.append(pdb->lastErrorMessage);
    //            QMessageBox::warning( this, applicationName, error );
    //            //not until nested transaction are supported
    //            //pdb->executeSQL(QString("ROLLBACK;"));
    //            setActiveTable(pdb, curTable);
    //}
}


void editTableForm::addField()
{
    editFieldForm * addForm = new editFieldForm( this );
    addForm->setModal(true);
    addForm->setInitialValues(pdb, true, curTable, QString(""),QString(""));
    if (addForm->exec())
    {
        QTreeWidgetItem *tbitem = new QTreeWidgetItem(ui->treeWidget);
        tbitem->setText( 0, addForm->field_name);
        tbitem->setText( 1, addForm->field_type);
        modified = true;
        ui->treeWidget->addTopLevelItem(tbitem);
    }
}


void editTableForm::removeField()
{
    // Is there any item selected to delete?
    if(!ui->treeWidget->currentItem())
        return;

    // Are we creating a new table or editing an old one?
    if(curTable == "")
    {
        // Creating a new one

        // Just delete that item. At this point there is no DB table to edit or data to be lost anyway
        delete ui->treeWidget->currentItem();
    } else {
        // Editing an old one

        // Ask user wether he really wants to delete that column
//      QString msg = tr("Are you sure you want to delete the field '%1'?\n All data currently stored in this field will be lost.").arg(ui->treeWidget->currentItem()->text(0));
//      if(QMessageBox::warning(this, QApplication::applicationName(), msg, QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape) == QMessageBox::Yes)
//      {
            // TODO
            QMessageBox::information(this, QApplication::applicationName(), tr("Sorry! This function is currently not implemented as SQLite does not support the deletion of columns yet."));
//      }
    }
}

void editTableForm::fieldSelectionChanged()
{
    ui->renameFieldButton->setEnabled(ui->treeWidget->selectionModel()->hasSelection());
    ui->removeFieldButton->setEnabled(ui->treeWidget->selectionModel()->hasSelection());
}
