//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";
import { useTranslation } from "react-i18next";
// import { PinIcon } from "lucide-react";

import { useWidgetStore, useDialogStore } from "@/store";
import { TerminalPopupContent } from "@/components/Popup/Terminal";
import { CustomNodeConnPopupContent } from "@/components/Popup/CustomNodeConn";
import { LogViewerPopupContent } from "@/components/Popup/LogViewer";
import { GraphPopupContent } from "@/components/Popup/Graph";
import { PopupTabContentDefault } from "@/components/Popup/Default";
import { ExtensionPopupContent } from "@/components/Popup/Default/Extension";
import { EditorPopupContent } from "@/components/Popup/Editor";
import {
  PopupBase,
  PopupTabsBar,
  PopupTabsBarItem,
  PopupTabsBarContent,
} from "@/components/Popup/Base";
import { groupWidgetsById } from "@/components/Popup/utils";
import {
  EWidgetCategory,
  EWidgetDisplayType,
  EWidgetPredefinedCheck,
  IWidget,
} from "@/types/widgets";
import { cn } from "@/lib/utils";
import { getCurrentWindowSize } from "@/utils";

const PopupWithTabs = (props: {
  widgets: IWidget[];
  groupId: string;
  containerId: string;
  size?: {
    width?: number;
    height?: number;
    windowWidth?: number;
    windowHeight?: number;
    initialPosition?: string;
  };
}) => {
  const { widgets, groupId, containerId, size } = props;

  const [activeWidgetId, setActiveWidgetId] = React.useState<string>(
    widgets[0].widget_id
  );

  const { removeWidgets, updateWidgetDisplayTypeBulk } = useWidgetStore();
  const { appendDialog, removeDialog } = useDialogStore();
  const { t } = useTranslation();

  const handleClose = async (type: "close" | "pin-to-dock" = "close") => {
    // Check the predefined checks
    const predefinedChecks = widgets
      .map((widget) => widget.actions?.checks)
      .filter((checks) => checks !== undefined)
      .flat();
    const uniqueChecks: {
      check: EWidgetPredefinedCheck;
      passed: boolean | undefined;
    }[] = [...new Set(predefinedChecks)].map((check) => ({
      check,
      passed: undefined,
    }));
    for (const check of uniqueChecks) {
      switch (check.check) {
        case EWidgetPredefinedCheck.EDITOR_UNSAVED_CHANGES:
          // eslint-disable-next-line no-case-declarations
          const editorWidgets = widgets.filter(
            (widget) => widget.category === EWidgetCategory.Editor
          );
          if (editorWidgets.length > 0) {
            const hasUnsavedChanges = editorWidgets.some(
              (widget) => widget.metadata.isContentChanged
            );
            check.passed = !hasUnsavedChanges;
          } else {
            check.passed = true;
          }
          break;
        default:
          break;
      }
    }
    if (
      uniqueChecks.some(
        (check) =>
          check.check === EWidgetPredefinedCheck.EDITOR_UNSAVED_CHANGES &&
          check.passed === false
      )
    ) {
      console.warn("Some widgets are not ready to be closed");
      const dialogId = containerId + groupId + "close-popup-dialog";
      appendDialog({
        id: dialogId,
        title: t("action.confirm"),
        content:
          type === "pin-to-dock"
            ? t("popup.editor.confirmPinToDock")
            : t("popup.editor.confirmCloseAllTabs"),
        confirmLabel:
          type === "pin-to-dock" ? t("action.pinToDock") : t("action.discard"),
        cancelLabel: t("action.cancel"),
        onConfirm: async () => {
          if (type === "pin-to-dock") {
            updateWidgetDisplayTypeBulk(
              widgets.map((widget) => widget.widget_id),
              EWidgetDisplayType.Dock
            );
            return;
          }
          removeDialog(dialogId);
        },
        postConfirm: async () => {
          if (type === "pin-to-dock") {
            return;
          }
          removeWidgets(widgets.map((widget) => widget.widget_id));
        },
      });
      return; // Do not execute the onClose actions
    }

    if (type === "pin-to-dock") {
      updateWidgetDisplayTypeBulk(
        widgets.map((widget) => widget.widget_id),
        EWidgetDisplayType.Dock
      );
      return;
    }

    // Execute the onClose actions
    const widgetOnCloseActions = widgets
      .map((widget) => widget.actions?.onClose)
      .filter((action) => action !== undefined);
    await Promise.all(widgetOnCloseActions.map((action) => action()));

    // Remove the widgets
    removeWidgets(widgets.map((widget) => widget.widget_id));
  };

  const handleSelectWidget = React.useCallback((widget_id: string) => {
    setActiveWidgetId(widget_id);
  }, []);

  const globalCustomActions = React.useMemo(() => {
    if (widgets.length > 1) {
      // return [
      //   {
      //     id: "pin-to-dock-" + groupId,
      //     label: t("action.pinToDock"),
      //     Icon: PinIcon,
      //     onClick: () => {
      //       handleClose("pin-to-dock");
      //     },
      //   },
      // ];
      return [];
    }
    const firstWidget = widgets[0];
    return firstWidget.actions?.custom_actions;
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [groupId, widgets]);

  const globalTitle = React.useMemo(() => {
    if (widgets.length > 1) return undefined;
    const firstWidget = widgets[0];
    return firstWidget.title;
  }, [widgets]);

  return (
    <PopupBase
      id={`${containerId}-${groupId}`}
      containerId={containerId}
      groupId={groupId}
      onClose={handleClose}
      customActions={globalCustomActions}
      title={globalTitle}
      contentClassName={cn("p-0 flex flex-col", {
        "h-full p-2": widgets.length === 1,
      })}
      defaultWidth={
        size?.width
          ? size?.width > 1
            ? size.width
            : size?.width * (size?.windowWidth || 1)
          : undefined
      }
      defaultHeight={
        size?.height
          ? size?.height > 1
            ? size?.height
            : size?.height * (size?.windowHeight || 1)
          : undefined
      }
      maxWidth={size?.windowWidth}
      maxHeight={size?.windowHeight}
      initialPosition={size?.initialPosition}
      onSelectWidget={handleSelectWidget}
    >
      <PopupTabs
        widgets={widgets}
        selectedWidgetId={activeWidgetId}
        onSelectWidget={handleSelectWidget}
      />
    </PopupBase>
  );
};

const WidgetContentRenderMappings: Record<
  EWidgetCategory,
  React.ComponentType<{ widget: IWidget }>
> = {
  [EWidgetCategory.Default]: PopupTabContentDefault as React.ComponentType<{
    widget: IWidget;
  }>,
  [EWidgetCategory.Extension]: ExtensionPopupContent as React.ComponentType<{
    widget: IWidget;
  }>,
  [EWidgetCategory.Graph]: GraphPopupContent as React.ComponentType<{
    widget: IWidget;
  }>,
  [EWidgetCategory.CustomConnection]:
    CustomNodeConnPopupContent as React.ComponentType<{ widget: IWidget }>,
  [EWidgetCategory.Terminal]: TerminalPopupContent as React.ComponentType<{
    widget: IWidget;
  }>,
  [EWidgetCategory.LogViewer]: LogViewerPopupContent as React.ComponentType<{
    widget: IWidget;
  }>,
  [EWidgetCategory.Editor]: EditorPopupContent as React.ComponentType<{
    widget: IWidget;
  }>,
};

const PopupTabs = (props: {
  widgets: IWidget[];
  selectedWidgetId: string;
  onSelectWidget: (widget_id: string) => void;
}) => {
  const { widgets, selectedWidgetId, onSelectWidget } = props;

  const { removeWidget } = useWidgetStore();

  const showTabsBar = React.useMemo(() => {
    return widgets?.length > 1;
  }, [widgets]);

  return (
    <>
      {showTabsBar && (
        <PopupTabsBar>
          {widgets.map((widget) => (
            <PopupTabsBarItem
              key={"PopupTabsBarItem" + widget.widget_id}
              id={widget.widget_id}
              isActive={widget.widget_id === selectedWidgetId}
              onSelect={onSelectWidget}
              onClose={() => {
                removeWidget(widget.widget_id);
                if (selectedWidgetId === widget.widget_id) {
                  const nextWidget = widgets.find(
                    (w) => w.widget_id !== widget.widget_id
                  );
                  if (nextWidget) {
                    onSelectWidget(nextWidget.widget_id);
                  }
                }
              }}
            >
              {widget.title}
            </PopupTabsBarItem>
          ))}
        </PopupTabsBar>
      )}
      {widgets.map((widget) => {
        const Renderer = WidgetContentRenderMappings[widget.category];

        if (!Renderer) return null;

        return (
          <PopupTabsBarContent
            key={"PopupTabsBarContent" + widget.widget_id}
            isActive={widget.widget_id === selectedWidgetId}
            fullHeight={widgets.length === 1}
          >
            <Renderer widget={widget} />
          </PopupTabsBarContent>
        );
      })}
    </>
  );
};

export function GlobalPopups() {
  const { widgets } = useWidgetStore();

  const currentWindowSizeMemo = React.useMemo(() => {
    const currentWindowSize = getCurrentWindowSize();
    return currentWindowSize;
  }, []);

  const groupedWidgets = React.useMemo(() => {
    return groupWidgetsById(widgets);
  }, [widgets]);

  const containerIds = React.useMemo(() => {
    return Object.keys(groupedWidgets);
  }, [groupedWidgets]);

  const getGroupByContainerId = React.useCallback(
    (containerId: string) => {
      const targetGroups = groupedWidgets[containerId];
      if (!targetGroups) {
        return [];
      }
      return Object.keys(targetGroups).map((groupId) => {
        return {
          groupId,
          widgets: targetGroups[groupId],
        };
      });
    },
    [groupedWidgets]
  );

  return (
    <>
      {containerIds.map((containerId) => (
        <React.Fragment key={containerId}>
          {getGroupByContainerId(containerId).map((groupObj) => {
            const firstWidgetSize = groupObj.widgets[0].popup;
            const size = {
              width: firstWidgetSize?.width,
              height: firstWidgetSize?.height,
              windowWidth: currentWindowSizeMemo?.width,
              windowHeight: currentWindowSizeMemo?.height,
              initialPosition: firstWidgetSize?.initialPosition,
            };

            return (
              <PopupWithTabs
                key={groupObj.groupId}
                widgets={groupObj.widgets}
                groupId={groupObj.groupId}
                containerId={containerId}
                size={size}
              />
            );
          })}
        </React.Fragment>
      ))}
    </>
  );
}
