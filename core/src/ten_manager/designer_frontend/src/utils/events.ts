//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { z } from "zod";

import { EConnectionType } from "@/types/graphs";

export enum EEventName {
  CustomNodeActionPopup = "customNodeActionPopup",
  BringToFront = "bringToFront",
}

export const customNodeActionPopupPayloadSchema = z.object({
  action: z.string(),
  source: z.string(),
  target: z.string().optional(),
  metadata: z
    .object({
      filters: z
        .object({
          type: z.nativeEnum(EConnectionType).optional(),
          source: z.boolean().optional(),
          target: z.boolean().optional(),
        })
        .optional(),
    })
    .optional(),
});

export const bringToFrontPayloadSchema = z.object({
  group_id: z.string().optional(),
  widget_id: z.string().optional(),
});

export const eventMappings = {
  [EEventName.CustomNodeActionPopup]: {
    id: EEventName.CustomNodeActionPopup,
    payload: customNodeActionPopupPayloadSchema,
  },
  [EEventName.BringToFront]: {
    id: EEventName.BringToFront,
    payload: bringToFrontPayloadSchema,
  },
};

/**
 * Event PubSub system for managing custom events
 */
export class EventPubSub {
  private static instance: EventPubSub;
  private subscribers: Map<
    EEventName,
    Array<{ id: string; callback: (data: unknown) => void }>
  >;
  private nextId: number = 0;

  private constructor() {
    this.subscribers = new Map();
  }

  public static getInstance(): EventPubSub {
    if (!EventPubSub.instance) {
      EventPubSub.instance = new EventPubSub();
    }
    return EventPubSub.instance;
  }

  /**
   * Generate a unique subscription ID
   */
  private generateId(): string {
    return `sub_${this.nextId++}`;
  }

  /**
   * Subscribe to an event
   * @param eventName The event to subscribe to
   * @param callback The callback to execute when the event is published
   * @param id Optional custom subscription ID
   * @returns A function to unsubscribe and the subscription ID
   */
  public subscribe<T extends EEventName>(
    eventName: T,
    callback: (data: z.infer<(typeof eventMappings)[T]["payload"]>) => void,
    id?: string
  ): { unsubscribe: () => void; id: string } {
    if (!this.subscribers.has(eventName)) {
      this.subscribers.set(eventName, []);
    }

    const subId = id || this.generateId();
    const subscribers = this.subscribers.get(eventName)!;
    subscribers.push({
      id: subId,
      callback: callback as (data: unknown) => void,
    });

    return {
      id: subId,
      unsubscribe: () => {
        this.unsubById(eventName, subId);
      },
    };
  }

  /**
   * Unsubscribe from an event by subscription ID
   * @param eventName The event to unsubscribe from
   * @param id The subscription ID
   */
  public unsubById(eventName: EEventName, id: string): void {
    const subscribers = this.subscribers.get(eventName);
    if (subscribers) {
      const index = subscribers.findIndex((sub) => sub.id === id);
      if (index !== -1) {
        subscribers.splice(index, 1);
      }
    }
  }

  /**
   * Unsubscribe a specific callback from an event
   * @param eventName The event to unsubscribe from
   * @param callback The callback to remove
   */
  public unsubscribe<T extends EEventName>(
    eventName: T,
    callback: (data: z.infer<(typeof eventMappings)[T]["payload"]>) => void
  ): void {
    const subscribers = this.subscribers.get(eventName);
    if (subscribers) {
      const index = subscribers.findIndex((sub) => sub.callback === callback);
      if (index !== -1) {
        subscribers.splice(index, 1);
      }
    }
  }

  /**
   * Publish an event
   * @param eventName The event to publish
   * @param data The data to pass to subscribers
   */
  public publish<T extends EEventName>(
    eventName: T,
    data: z.infer<(typeof eventMappings)[T]["payload"]>
  ): void {
    // Validate the data against the schema
    try {
      eventMappings[eventName].payload.parse(data);
    } catch (error) {
      console.error(`Invalid payload for event ${eventName}:`, error);
      return;
    }

    const subscribers = this.subscribers.get(eventName);
    if (subscribers) {
      subscribers.forEach((sub) => sub.callback(data));
    }
  }

  /**
   * Unsubscribe all callbacks for an event
   * @param eventName The event to unsubscribe from
   */
  public unsubscribeAll(eventName: EEventName): void {
    this.subscribers.delete(eventName);
  }

  /**
   * Dispatch a DOM custom event
   * @param eventName The event to dispatch
   * @param data The data to include in the event
   */
  public dispatchDOMEvent<T extends EEventName>(
    eventName: T,
    data: z.infer<(typeof eventMappings)[T]["payload"]>
  ): void {
    if (typeof window !== "undefined") {
      window.dispatchEvent(
        new CustomEvent(eventName, {
          detail: data,
        })
      );
    }
  }
}

// Export a singleton instance
export const eventPubSub = EventPubSub.getInstance();

// --- Dispatch Custom Event ---

export const dispatchBringToFront = (
  options: z.infer<typeof bringToFrontPayloadSchema>
) => {
  eventPubSub.publish(EEventName.BringToFront, options);
};

export const dispatchCustomNodeActionPopup = (
  options: z.infer<typeof customNodeActionPopupPayloadSchema>
) => {
  eventPubSub.publish(EEventName.CustomNodeActionPopup, options);
};
