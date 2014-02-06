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
package org.netxms.ui.eclipse.perfview.propertypages;

import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Group;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Scale;
import org.eclipse.swt.widgets.Spinner;
import org.eclipse.ui.dialogs.PropertyPage;
import org.netxms.client.NXCSession;
import org.netxms.client.datacollection.GraphSettings;
import org.netxms.ui.eclipse.jobs.ConsoleJob;
import org.netxms.ui.eclipse.perfview.Activator;
import org.netxms.ui.eclipse.perfview.ChartConfig;
import org.netxms.ui.eclipse.perfview.Messages;
import org.netxms.ui.eclipse.perfview.PredefinedChartConfig;
import org.netxms.ui.eclipse.perfview.widgets.YAxisRangeEditor;
import org.netxms.ui.eclipse.shared.ConsoleSharedData;
import org.netxms.ui.eclipse.tools.WidgetFactory;
import org.netxms.ui.eclipse.tools.WidgetHelper;
import org.netxms.ui.eclipse.widgets.DateTimeSelector;
import org.netxms.ui.eclipse.widgets.LabeledText;

/**
 * "General" property page for chart
 */
public class General extends PropertyPage
{
	private ChartConfig config;
	private LabeledText title;
	private Button checkShowGrid;
	private Button checkShowLegend;
	private Button checkShowHostNames;
	private Button checkAutoRefresh;
	private Button checkLogScale;
	private Scale refreshIntervalScale;
	private Spinner refreshIntervalSpinner;
	private Button radioBackFromNow;
	private Button radioFixedInterval;
	private Spinner timeRange;
	private Combo timeUnits;
	private DateTimeSelector timeFrom;
	private DateTimeSelector timeTo;
	private YAxisRangeEditor yAxisRange;
	
	/* (non-Javadoc)
	 * @see org.eclipse.jface.preference.PreferencePage#createContents(org.eclipse.swt.widgets.Composite)
	 */
	@Override
	protected Control createContents(Composite parent)
	{
		config = (ChartConfig)getElement().getAdapter(ChartConfig.class);
		
		Composite dialogArea = new Composite(parent, SWT.NONE);
		
		GridLayout layout = new GridLayout();
		layout.verticalSpacing = WidgetHelper.OUTER_SPACING;
		layout.marginWidth = 0;
		layout.marginHeight = 0;
      dialogArea.setLayout(layout);
      
      title = new LabeledText(dialogArea, SWT.NONE, SWT.BORDER);
      title.setLabel(Messages.get().General_Title);
      title.setText(config.getTitle());
      GridData gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      title.setLayoutData(gd);
      
      Group optionsGroup = new Group(dialogArea, SWT.NONE);
      optionsGroup.setText(Messages.get().General_Options);
      layout = new GridLayout();
      layout.marginWidth = WidgetHelper.OUTER_SPACING;
      layout.marginHeight = WidgetHelper.OUTER_SPACING;
      layout.verticalSpacing = WidgetHelper.OUTER_SPACING;
      layout.makeColumnsEqualWidth = true;
      layout.numColumns = 2;
      optionsGroup.setLayout(layout);
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      optionsGroup.setLayoutData(gd);
      
      checkShowGrid = new Button(optionsGroup, SWT.CHECK);
      checkShowGrid.setText(Messages.get().General_ShowGridLines);
      checkShowGrid.setSelection(config.isShowGrid());

      checkShowLegend = new Button(optionsGroup, SWT.CHECK);
      checkShowLegend.setText(Messages.get().General_ShowLegend);
      checkShowLegend.setSelection(config.isShowLegend());

      checkShowHostNames = new Button(optionsGroup, SWT.CHECK);
      checkShowHostNames.setText(Messages.get().General_ShowHostNames);
      checkShowHostNames.setSelection(config.isShowHostNames());

      checkAutoRefresh = new Button(optionsGroup, SWT.CHECK);
      checkAutoRefresh.setText(Messages.get().General_Autorefresh);
      checkAutoRefresh.setSelection(config.isAutoRefresh());

      checkLogScale = new Button(optionsGroup, SWT.CHECK);
      checkLogScale.setText(Messages.get().General_LogScale);
      checkLogScale.setSelection(config.isLogScale());
      
      Composite refreshIntervalGroup = new Composite(optionsGroup, SWT.NONE);
      layout = new GridLayout();
      layout.numColumns = 2;
      layout.horizontalSpacing = WidgetHelper.OUTER_SPACING;
      layout.marginHeight = 0;
      layout.marginWidth = 0;
      layout.marginTop = WidgetHelper.OUTER_SPACING;
      refreshIntervalGroup.setLayout(layout);
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.horizontalSpan = 2;
      refreshIntervalGroup.setLayoutData(gd);
      
      Label label = new Label(refreshIntervalGroup, SWT.NONE);
      label.setText(Messages.get().General_RefreshInterval);
      gd = new GridData();
      gd.horizontalAlignment = SWT.LEFT;
      gd.horizontalSpan = 2;
      label.setLayoutData(gd);
      
      refreshIntervalScale = new Scale(refreshIntervalGroup, SWT.HORIZONTAL);
      refreshIntervalScale.setMinimum(1);
      refreshIntervalScale.setMaximum(600);
      refreshIntervalScale.setSelection(config.getRefreshRate());
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      refreshIntervalScale.setLayoutData(gd);
      refreshIntervalScale.addSelectionListener(new SelectionListener() {
			@Override
			public void widgetSelected(SelectionEvent e)
			{
				refreshIntervalSpinner.setSelection(refreshIntervalScale.getSelection());
			}

			@Override
			public void widgetDefaultSelected(SelectionEvent e)
			{
				widgetSelected(e);
			}
      });
      
      refreshIntervalSpinner = new Spinner(refreshIntervalGroup, SWT.BORDER);
      refreshIntervalSpinner.setMinimum(1);
      refreshIntervalSpinner.setMaximum(600);
      refreshIntervalSpinner.setSelection(config.getRefreshRate());
      refreshIntervalSpinner.addSelectionListener(new SelectionListener() {
			@Override
			public void widgetSelected(SelectionEvent e)
			{
				refreshIntervalScale.setSelection(refreshIntervalSpinner.getSelection());
			}
			
			@Override
			public void widgetDefaultSelected(SelectionEvent e)
			{
				widgetSelected(e);
			}
		});
      
      Group timeGroup = new Group(dialogArea, SWT.NONE);
      timeGroup.setText(Messages.get().General_TimePeriod);
      layout = new GridLayout();
      layout.marginWidth = WidgetHelper.OUTER_SPACING;
      layout.marginHeight = WidgetHelper.OUTER_SPACING;
      layout.horizontalSpacing = 16;
      layout.makeColumnsEqualWidth = true;
      layout.numColumns = 2;
      timeGroup.setLayout(layout);
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      timeGroup.setLayoutData(gd);
      
      final SelectionListener listener = new SelectionListener() {
			@Override
			public void widgetSelected(SelectionEvent e)
			{
				timeRange.setEnabled(radioBackFromNow.getSelection());
				timeUnits.setEnabled(radioBackFromNow.getSelection());
		      timeFrom.setEnabled(radioFixedInterval.getSelection());
		      timeTo.setEnabled(radioFixedInterval.getSelection());
			}
			
			@Override
			public void widgetDefaultSelected(SelectionEvent e)
			{
				widgetSelected(e);
			}
		};

      radioBackFromNow = new Button(timeGroup, SWT.RADIO);
      radioBackFromNow.setText(Messages.get().General_TimePeriodBack);
      radioBackFromNow.setSelection(config.getTimeFrameType() == GraphSettings.TIME_FRAME_BACK_FROM_NOW);
      radioBackFromNow.addSelectionListener(listener);
      
      radioFixedInterval = new Button(timeGroup, SWT.RADIO);
      radioFixedInterval.setText(Messages.get().General_TimePeriodFixed);
      radioFixedInterval.setSelection(config.getTimeFrameType() == GraphSettings.TIME_FRAME_FIXED);
      radioFixedInterval.addSelectionListener(listener);
      
      Composite timeBackGroup = new Composite(timeGroup, SWT.NONE);
      layout = new GridLayout();
      layout.marginWidth = 0;
      layout.marginHeight = 0;
      layout.horizontalSpacing = WidgetHelper.OUTER_SPACING;
      layout.numColumns = 2;
      timeBackGroup.setLayout(layout);
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.verticalAlignment = SWT.TOP;
      timeBackGroup.setLayoutData(gd);
      
		timeRange = WidgetHelper.createLabeledSpinner(timeBackGroup, SWT.BORDER, Messages.get().General_TimeInterval, 1, 10000, WidgetHelper.DEFAULT_LAYOUT_DATA);
		timeRange.setSelection(config.getTimeRange());
		timeRange.setEnabled(radioBackFromNow.getSelection());
		
		timeUnits = WidgetHelper.createLabeledCombo(timeBackGroup, SWT.READ_ONLY, Messages.get().General_TimeUnits, WidgetHelper.DEFAULT_LAYOUT_DATA);
		timeUnits.add(Messages.get().General_TimeUnitMinutes);
		timeUnits.add(Messages.get().General_TimeUnitHours);
		timeUnits.add(Messages.get().General_TimeUnitDays);
		timeUnits.select(config.getTimeUnits());
		timeUnits.setEnabled(radioBackFromNow.getSelection());

      Composite timeFixedGroup = new Composite(timeGroup, SWT.NONE);
      layout = new GridLayout();
      layout.marginWidth = 0;
      layout.marginHeight = 0;
      layout.verticalSpacing = WidgetHelper.OUTER_SPACING;
      timeFixedGroup.setLayout(layout);
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.verticalAlignment = SWT.TOP;
      timeFixedGroup.setLayoutData(gd);
      
      final WidgetFactory factory = new WidgetFactory() {
			@Override
			public Control createControl(Composite parent, int style)
			{
				return new DateTimeSelector(parent, style);
			}
		};
		
      timeFrom = (DateTimeSelector)WidgetHelper.createLabeledControl(timeFixedGroup, SWT.NONE, factory, Messages.get().General_TimeFrom, WidgetHelper.DEFAULT_LAYOUT_DATA);
      timeFrom.setValue(config.getTimeFrom());
      timeFrom.setEnabled(radioFixedInterval.getSelection());

      timeTo = (DateTimeSelector)WidgetHelper.createLabeledControl(timeFixedGroup, SWT.NONE, factory, Messages.get().General_TimeTo, WidgetHelper.DEFAULT_LAYOUT_DATA);
      timeTo.setValue(config.getTimeTo());
      timeTo.setEnabled(radioFixedInterval.getSelection());

      yAxisRange = new YAxisRangeEditor(dialogArea, SWT.NONE);
      gd = new GridData();
      gd.horizontalSpan = layout.numColumns;
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      yAxisRange.setLayoutData(gd);
      yAxisRange.setSelection(config.isAutoScale(), config.getMinYScaleValue(), config.getMaxYScaleValue());
      
      return dialogArea;
	}

	/* (non-Javadoc)
	 * @see org.eclipse.jface.preference.PreferencePage#performDefaults()
	 */
	@Override
	protected void performDefaults()
	{
		super.performDefaults();
		
		title.setText(""); //$NON-NLS-1$
		checkShowGrid.setSelection(true);
		checkShowLegend.setSelection(true);
		checkShowHostNames.setSelection(false);
		checkAutoRefresh.setSelection(true);
		checkLogScale.setSelection(false);
		
		yAxisRange.setSelection(true, 0, 100);
		
		refreshIntervalScale.setSelection(30);
		refreshIntervalSpinner.setSelection(30);
		
		radioBackFromNow.setSelection(true);
		radioFixedInterval.setSelection(false);
		
		timeRange.setSelection(60);
		timeRange.setEnabled(true);
		timeUnits.select(0);
		timeUnits.setEnabled(true);
		timeFrom.setEnabled(false);
		timeTo.setEnabled(false);
	}

	/**
	 * Apply changes
	 * 
	 * @param isApply true if update operation caused by "Apply" button
	 */
	protected void applyChanges(final boolean isApply)
	{
		config.setTitle(title.getText());
		config.setShowGrid(checkShowGrid.getSelection());
		config.setShowLegend(checkShowLegend.getSelection());
		config.setAutoScale(yAxisRange.isAuto());
		config.setShowHostNames(checkShowHostNames.getSelection());
		config.setAutoRefresh(checkAutoRefresh.getSelection());
		config.setLogScale(checkLogScale.getSelection());
		config.setRefreshRate(refreshIntervalSpinner.getSelection());
		
		config.setTimeFrameType(radioBackFromNow.getSelection() ? GraphSettings.TIME_FRAME_BACK_FROM_NOW : GraphSettings.TIME_FRAME_FIXED);
		config.setTimeUnits(timeUnits.getSelectionIndex());
		config.setTimeRange(timeRange.getSelection());
		config.setTimeFrom(timeFrom.getValue());
		config.setTimeTo(timeTo.getValue());
		
		config.setMinYScaleValue(yAxisRange.getMinY());
		config.setMaxYScaleValue(yAxisRange.getMaxY());
		
		if ((config instanceof PredefinedChartConfig) && isApply)
		{
			setValid(false);
			final NXCSession session = (NXCSession)ConsoleSharedData.getSession();
			new ConsoleJob(Messages.get().General_JobName, null, Activator.PLUGIN_ID, null) {
				@Override
				protected void runInternal(IProgressMonitor monitor) throws Exception
				{
					session.saveGraph(((PredefinedChartConfig)config).createServerSettings(), true);
					runInUIThread(new Runnable() {
						@Override
						public void run()
						{
							General.this.setValid(true);
						}
					});
				}
				
				@Override
				protected String getErrorMessage()
				{
					return Messages.get().General_JobError;
				}
			}.start();
		}
	}
	
	/* (non-Javadoc)
	 * @see org.eclipse.jface.preference.PreferencePage#performApply()
	 */
	@Override
	protected void performApply()
	{
		applyChanges(true);
	}

	/* (non-Javadoc)
	 * @see org.eclipse.jface.preference.PreferencePage#performOk()
	 */
	@Override
	public boolean performOk()
	{
		applyChanges(false);
		return true;
	}
}
