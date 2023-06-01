/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2022 Victor Kirhenshtein
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
package org.netxms.nxmc.modules.logviewer.views;

import org.eclipse.jface.action.IMenuManager;
import org.eclipse.jface.action.IToolBarManager;
import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.widgets.Composite;
import org.netxms.client.NXCSession;
import org.netxms.nxmc.Registry;
import org.netxms.nxmc.base.views.View;
import org.netxms.nxmc.modules.logviewer.LogRecordDetailsViewer;
import org.netxms.nxmc.modules.logviewer.LogRecordDetailsViewerRegistry;
import org.netxms.nxmc.modules.logviewer.widgets.LogWidget;
import org.netxms.nxmc.resources.ResourceManager;

/**
 * Universal log viewer
 */
public class LogViewer extends View
{
   protected NXCSession session = Registry.getSession();
   private LogWidget logWidget;
   private String logName;
   private LogRecordDetailsViewer recordDetailsViewer;
   
   /**
    * Internal constructor used for cloning
    */
   protected LogViewer()
   {
      super();
   }

   /**
    * @see org.netxms.nxmc.base.views.View#cloneView()
    */
   @Override
   public View cloneView()
   {
      LogViewer view = (LogViewer)super.cloneView();
      
      view.logName = logName;
      view.recordDetailsViewer = recordDetailsViewer;
      return view;
   }

   /**
    * Create new log viewer view
    *
    * @param viewName view name
    * @param logName log name
    */
   public LogViewer(String viewName, String logName)
   {
      super(viewName, ResourceManager.getImageDescriptor("icons/log-viewer/" + logName + ".png"), "LogViewer." + logName, false);
      this.logName = logName;
      recordDetailsViewer = LogRecordDetailsViewerRegistry.get(logName);
   }

   /**
    * @see org.netxms.nxmc.base.views.View#createContent(org.eclipse.swt.widgets.Composite)
    */
	@Override
   public void createContent(Composite parent)
	{
		parent.setLayout(new FillLayout());		
		logWidget = new LogWidget(this, parent, SWT.NONE, logName);
   }

	
	
   /**
    * @see org.netxms.nxmc.base.views.View#postClone(org.netxms.nxmc.base.views.View)
    */
   @Override
   protected void postClone(View view)
   {
      LogViewer origin = (LogViewer)view;
      logWidget.postClone(origin.logWidget);
      super.postClone(view);
   }

   /**
    * @see org.netxms.nxmc.base.views.View#postContentCreate()
    */
   @Override
   protected void postContentCreate()
   {
      super.postContentCreate();
      logWidget.openServerLog();
   }

   /**
    * @see org.netxms.nxmc.base.views.View#fillLocalMenu(org.eclipse.jface.action.IMenuManager)
    */
   @Override
   protected void fillLocalMenu(IMenuManager manager)
	{
      logWidget.fillLocalMenu(manager);
	}

   /**
    * @see org.netxms.nxmc.base.views.View#fillLocalToolBar(org.eclipse.jface.action.IToolBarManager)
    */
   @Override
   protected void fillLocalToolBar(IToolBarManager manager)
	{
      logWidget.fillLocalToolBar(manager);
	}

	/**
    * @see org.netxms.nxmc.base.views.View#refresh()
    */
   @Override
   public void refresh()
	{
      logWidget.refresh();
	}

   /**
    * @see org.netxms.nxmc.base.views.View#setFocus()
    */
	@Override
	public void setFocus()
	{
	   logWidget.getViewer().getControl().setFocus();
	}
}
