/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2013 Victor Kirhenshtein
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
package org.netxms.nxmc.modules.objects.dialogs;

import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Shell;
import org.netxms.nxmc.base.widgets.LabeledText;
import org.netxms.nxmc.localization.LocalizationHelper;
import org.netxms.nxmc.tools.MessageDialogHelper;
import org.netxms.nxmc.tools.WidgetHelper;
import org.xnap.commons.i18n.I18n;

/**
 * Zone object creation dialog
 *
 */
public class CreateZoneDialog extends Dialog
{
   private final I18n i18n = LocalizationHelper.getI18n(CreateZoneDialog.class);

	private LabeledText nameField;
   private LabeledText aliasField;
	private LabeledText uinField;

	private String name;
   private String alias;
	private int zoneUIN;

	/**
	 * @param parentShell
	 */
	public CreateZoneDialog(Shell parentShell)
	{
		super(parentShell);
	}

   /**
    * @see org.eclipse.jface.window.Window#configureShell(org.eclipse.swt.widgets.Shell)
    */
	@Override
	protected void configureShell(Shell newShell)
	{
		super.configureShell(newShell);
      newShell.setText(i18n.tr("Create Zone"));
	}

   /**
    * @see org.eclipse.jface.dialogs.Dialog#createDialogArea(org.eclipse.swt.widgets.Composite)
    */
	@Override
	protected Control createDialogArea(Composite parent)
	{
		Composite dialogArea = (Composite)super.createDialogArea(parent);
		
		GridLayout layout = new GridLayout();
		layout.verticalSpacing = WidgetHelper.DIALOG_SPACING;
		layout.marginHeight = WidgetHelper.DIALOG_HEIGHT_MARGIN;
		layout.marginWidth = WidgetHelper.DIALOG_WIDTH_MARGIN;
		dialogArea.setLayout(layout);
		
		nameField = new LabeledText(dialogArea, SWT.NONE);
      nameField.setLabel(i18n.tr("Name"));
		nameField.getTextControl().setTextLimit(255);
		GridData gd = new GridData();
		gd.horizontalAlignment = SWT.FILL;
		gd.grabExcessHorizontalSpace = true;
		gd.widthHint = 300;
		nameField.setLayoutData(gd);

      aliasField = new LabeledText(dialogArea, SWT.NONE);
      aliasField.setLabel(i18n.tr("Alias"));
      aliasField.getTextControl().setTextLimit(255);
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      aliasField.setLayoutData(gd);

		uinField = new LabeledText(dialogArea, SWT.NONE);
      uinField.setLabel(i18n.tr("Zone UIN (leave empty for autogenerated UIN)"));
		uinField.getTextControl().setTextLimit(10);
		gd = new GridData();
		gd.horizontalAlignment = SWT.FILL;
		gd.grabExcessHorizontalSpace = true;
		uinField.setLayoutData(gd);
		
		return dialogArea;
	}

   /**
    * @see org.eclipse.jface.dialogs.Dialog#okPressed()
    */
	@Override
	protected void okPressed()
	{
	   String zoneIdText = uinField.getText().trim();
	   if (!zoneIdText.isEmpty())
	   {
   		try
   		{
   			zoneUIN = Integer.parseInt(uinField.getText());
   		}
   		catch(NumberFormatException e)
   		{
            MessageDialogHelper.openWarning(getShell(), i18n.tr("Warning"), i18n.tr("Zone UIN must be positive integer"));
   			return;
   		}
   		if (zoneUIN <= 0)
   		{
            MessageDialogHelper.openWarning(getShell(), i18n.tr("Warning"), i18n.tr("Zone UIN must be positive integer"));
   			return;
   		}
	   }
	   else
	   {
	      zoneUIN = 0;
	   }

		name = nameField.getText().trim();
      alias = aliasField.getText().trim();
		if (name.isEmpty())
		{
         MessageDialogHelper.openWarning(getShell(), i18n.tr("Warning"), i18n.tr("Please provide non-empty object name"));
			return;
		}

		super.okPressed();
	}

	/**
	 * @return the name
	 */
	public String getName()
	{
		return name;
	}

   /**
    * @return the alias
    */
   public String getAlias()
   {
      return alias;
   }

	/**
	 * Get zone UIN
	 * 
	 * @return zone UIN
	 */
	public int getZoneUIN()
	{
		return zoneUIN;
	}
}
