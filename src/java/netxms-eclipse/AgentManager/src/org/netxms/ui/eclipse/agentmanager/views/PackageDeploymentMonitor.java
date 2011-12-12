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
package org.netxms.ui.eclipse.agentmanager.views;

import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.part.ViewPart;
import org.netxms.ui.eclipse.agentmanager.views.helpers.DeploymentStatusComparator;
import org.netxms.ui.eclipse.agentmanager.views.helpers.DeploymentStatusLabelProvider;
import org.netxms.ui.eclipse.widgets.SortableTableViewer;

/**
 * Package deployment monitor
 */
public class PackageDeploymentMonitor extends ViewPart
{
	public static final String ID = "org.netxms.ui.eclipse.agentmanager.views.PackageDeploymentMonitor";
	
	public static final int COLUMN_NODE = 0;
	public static final int COLUMN_STATUS = 1;
	public static final int COLUMN_ERROR = 2;
	
	private SortableTableViewer viewer; 
	
	/* (non-Javadoc)
	 * @see org.eclipse.ui.part.WorkbenchPart#createPartControl(org.eclipse.swt.widgets.Composite)
	 */
	@Override
	public void createPartControl(Composite parent)
	{
		final String[] names = { "Node", "Status", "Message" };
		final int[] widths = { 150, 110, 400 };
		viewer = new SortableTableViewer(parent, names, widths, COLUMN_NODE, SWT.UP, SWT.FULL_SELECTION | SWT.MULTI);
		viewer.setContentProvider(new ArrayContentProvider());
		viewer.setLabelProvider(new DeploymentStatusLabelProvider());
		viewer.setComparator(new DeploymentStatusComparator());
	}

	/* (non-Javadoc)
	 * @see org.eclipse.ui.part.WorkbenchPart#setFocus()
	 */
	@Override
	public void setFocus()
	{
		viewer.getTable().setFocus();
	}

	/**
	 * @param nodeId
	 * @param status
	 * @param message
	 */
	public void statusUpdate(long nodeId, int status, String message)
	{
		
	}
}
