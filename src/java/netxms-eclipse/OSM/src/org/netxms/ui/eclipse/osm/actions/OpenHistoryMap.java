/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2011 Victor Kirhenshtein
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
package org.netxms.ui.eclipse.osm.actions;

import org.eclipse.jface.action.IAction;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.ui.IObjectActionDelegate;
import org.eclipse.ui.IWorkbenchPage;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.IWorkbenchWindow;
import org.eclipse.ui.PartInitException;
import org.netxms.base.GeoLocation;
import org.netxms.client.objects.AbstractObject;
import org.netxms.ui.eclipse.osm.views.HistoryView;
import org.netxms.ui.eclipse.tools.MessageDialogHelper;

/**
 * Object action: open geolocation view
 */
public class OpenHistoryMap implements IObjectActionDelegate
{
	private IWorkbenchWindow window;
	private AbstractObject object;
	
	/* (non-Javadoc)
	 * @see org.eclipse.ui.IObjectActionDelegate#setActivePart(org.eclipse.jface.action.IAction, org.eclipse.ui.IWorkbenchPart)
	 */
	@Override
	public void setActivePart(IAction action, IWorkbenchPart targetPart)
	{
		window = targetPart.getSite().getWorkbenchWindow();
	}

	/* (non-Javadoc)
	 * @see org.eclipse.ui.IActionDelegate#run(org.eclipse.jface.action.IAction)
	 */
	@Override
	public void run(IAction action)
	{
		if(window != null)
		{	
		   //Open dialog
			try 
			{
				window.getActivePage().showView(HistoryView.ID, Long.toString(object.getObjectId()), IWorkbenchPage.VIEW_ACTIVATE);
			} 
			catch (PartInitException e) 
			{
				MessageDialogHelper.openError(window.getShell(), "Error while opening historical map", "Error while opening historical map: " + e.getMessage());
			}
		}
	}

	/* (non-Javadoc)
	 * @see org.eclipse.ui.IActionDelegate#selectionChanged(org.eclipse.jface.action.IAction, org.eclipse.jface.viewers.ISelection)
	 */
	@Override
	public void selectionChanged(IAction action, ISelection selection)
	{
		Object obj;
		if ((selection instanceof IStructuredSelection) &&
			 ((obj = ((IStructuredSelection)selection).getFirstElement()) instanceof AbstractObject))
		{
			object = (AbstractObject)obj;
		}
		else
		{
			object = null;
		}
		action.setEnabled((object != null) && (object.getGeolocation().getType() != GeoLocation.UNSET));
	}
}
