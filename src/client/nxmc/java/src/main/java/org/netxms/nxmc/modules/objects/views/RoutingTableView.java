/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2014 Victor Kirhenshtein
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
package org.netxms.nxmc.modules.objects.views;

import java.util.List;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.action.Action;
import org.eclipse.jface.action.IMenuListener;
import org.eclipse.jface.action.IMenuManager;
import org.eclipse.jface.action.IToolBarManager;
import org.eclipse.jface.action.MenuManager;
import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.DisposeEvent;
import org.eclipse.swt.events.DisposeListener;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Menu;
import org.netxms.client.NXCSession;
import org.netxms.client.objects.AbstractObject;
import org.netxms.client.objects.Node;
import org.netxms.client.topology.Route;
import org.netxms.nxmc.Registry;
import org.netxms.nxmc.base.actions.CopyTableRowsAction;
import org.netxms.nxmc.base.actions.ExportToCsvAction;
import org.netxms.nxmc.base.jobs.Job;
import org.netxms.nxmc.base.views.View;
import org.netxms.nxmc.base.widgets.SortableTableViewer;
import org.netxms.nxmc.localization.LocalizationHelper;
import org.netxms.nxmc.modules.objects.views.helpers.RoutingTableComparator;
import org.netxms.nxmc.modules.objects.views.helpers.RoutingTableLabelProvider;
import org.netxms.nxmc.resources.ResourceManager;
import org.netxms.nxmc.tools.WidgetHelper;
import org.xnap.commons.i18n.I18n;

/**
 * List of IP routes
 */
public class RoutingTableView extends ObjectView
{
   private static final I18n i18n = LocalizationHelper.getI18n(RoutingTableView.class);
	
	public static final int COLUMN_DESTINATION = 0;
	public static final int COLUMN_NEXT_HOP = 1;
	public static final int COLUMN_INTERFACE = 2;
	public static final int COLUMN_TYPE = 3;
	
	private NXCSession session;
	private SortableTableViewer viewer;
	private Action actionExportToCsv;
	private Action actionExportAllToCsv;
   private Action actionCopyRowToClipboard;
   

   /**
    * Create "Ports" view
    */
   public RoutingTableView()
   {
      super(i18n.tr("Routing Table"), ResourceManager.getImageDescriptor("icons/object-views/routing_table.gif"), "RoutingTable", false);
      session = Registry.getSession();
   }
   
   /**
    * @see org.netxms.nxmc.base.views.View#postClone(org.netxms.nxmc.base.views.View)
    */
   @Override
   protected void postClone(View view)
   {
      super.postClone(view);
      refresh();
   }

   /**
    * @see org.netxms.nxmc.modules.objects.views.ObjectView#isValidForContext(java.lang.Object)
    */
   @Override
   public boolean isValidForContext(Object context)
   {
      return (context != null) && (context instanceof Node) && (((Node)context).hasAgent() || ((Node)context).hasSnmpAgent());
   }

   /**
    * @see org.netxms.nxmc.base.views.View#getPriority()
    */
   @Override
   public int getPriority()
   {
      return 170;
   }

   /**
    * @see org.netxms.nxmc.base.views.View#createContent(org.eclipse.swt.widgets.Composite)
    */
   @Override
   protected void createContent(Composite parent)
   {
		final String[] names = { i18n.tr("Destination"), i18n.tr("Next hop"), i18n.tr("Interface"), i18n.tr("Type") };
		final int[] widths = { 180, 140, 200, 140 };
		viewer = new SortableTableViewer(parent, names, widths, COLUMN_DESTINATION, SWT.DOWN, SWT.FULL_SELECTION | SWT.MULTI);
		viewer.setContentProvider(new ArrayContentProvider());
		viewer.setLabelProvider(new RoutingTableLabelProvider());
		viewer.setComparator(new RoutingTableComparator());
		
		WidgetHelper.restoreTableViewerSettings(viewer, "RoutingTable"); //$NON-NLS-1$
		viewer.getTable().addDisposeListener(new DisposeListener() {
			@Override
			public void widgetDisposed(DisposeEvent e)
			{
				WidgetHelper.saveTableViewerSettings(viewer, "RoutingTable"); //$NON-NLS-1$
			}
		});

		createActions();
		createPopupMenu();
	}
	/**
	 * Create actions
	 */
	private void createActions()
	{
      actionCopyRowToClipboard = new CopyTableRowsAction(viewer, true);
		actionExportToCsv = new ExportToCsvAction(this, viewer, true);
		actionExportAllToCsv = new ExportToCsvAction(this, viewer, false);
	}

   /**
    * @see org.netxms.nxmc.base.views.View#fillLocalMenu(org.eclipse.jface.action.IMenuManager)
    */
   @Override
   protected void fillLocalMenu(IMenuManager manager)
	{
		manager.add(actionExportAllToCsv);
	}

	/**
	 * @see org.netxms.nxmc.base.views.View#fillLocalToolBar(org.eclipse.jface.action.IToolBarManager)
	 */
	@Override
	protected void fillLocalToolBar(IToolBarManager manager)
	{
		manager.add(actionExportAllToCsv);
	}
	
	/**
	 * Create pop-up menu
	 */
	private void createPopupMenu()
	{
		// Create menu manager.
		MenuManager menuMgr = new MenuManager();
		menuMgr.setRemoveAllWhenShown(true);
		menuMgr.addMenuListener(new IMenuListener() {
			public void menuAboutToShow(IMenuManager mgr)
			{
				fillContextMenu(mgr);
			}
		});

		// Create menu.
		Menu menu = menuMgr.createContextMenu(viewer.getControl());
		viewer.getControl().setMenu(menu);
	}

	/**
	 * Fill context menu
	 * @param mgr Menu manager
	 */
	protected void fillContextMenu(IMenuManager manager)
	{
      manager.add(actionCopyRowToClipboard);
		manager.add(actionExportToCsv);
	}

   /**
    * @see org.netxms.ui.eclipse.objectview.objecttabs.ObjectTab#refresh()
    */
   @Override
   public void refresh()
   {
      final String objectName = session.getObjectName(getObjectId());
	   new Job(i18n.tr("Read routing table"), this) {
         @Override
         protected void run(IProgressMonitor monitor) throws Exception
         {
            final List<Route> rt = session.getRoutingTable(getObjectId());
            runInUIThread(new Runnable() {
               @Override
               public void run()
               {
                  viewer.setInput(rt.toArray());
               }
            });
         }
         
         @Override
         protected String getErrorMessage()
         {
            return String.format(i18n.tr("Cannot get routing table for node %s"), objectName);
         }
      }.start();
	}

   /**
    * @see org.netxms.nxmc.modules.objects.views.ObjectView#onObjectChange(org.netxms.client.objects.AbstractObject)
    */
   @Override
   protected void onObjectChange(AbstractObject object)
   {
      viewer.setInput(new Route[0]);
      if (object != null)
         refresh();
   }
}
