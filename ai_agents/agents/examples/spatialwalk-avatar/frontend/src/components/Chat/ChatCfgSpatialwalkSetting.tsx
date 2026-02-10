import { zodResolver } from "@hookform/resolvers/zod";
import { LoaderCircleIcon, UsersIcon } from "lucide-react";
import * as React from "react";
import { useForm } from "react-hook-form";
import { toast } from "sonner";
import { z } from "zod";
import { useAppDispatch, useAppSelector } from "@/common/hooks";
import { Button, buttonVariants } from "@/components/ui/button";
import {
  Form,
  FormControl,
  FormField,
  FormItem,
  FormLabel,
} from "@/components/ui/form";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import {
  Sheet,
  SheetContent,
  SheetDescription,
  SheetHeader,
  SheetTitle,
  SheetTrigger,
} from "@/components/ui/sheet";
import { Switch } from "@/components/ui/switch";
import { cn } from "@/lib/utils";
import { setSpatialwalkSettings } from "@/store/reducers/global";
import { Input } from "../ui/input";

export function SpatialwalkCfgSheet() {
  const dispatch = useAppDispatch();
  const spatialwalkSettings = useAppSelector(
    (state) => state.global.spatialwalkSettings
  );

  return (
    <Sheet>
      <SheetTrigger
        className={cn(
          buttonVariants({ variant: "outline", size: "icon" }),
          "bg-transparent"
        )}
      >
        <UsersIcon />
      </SheetTrigger>
      <SheetContent className="w-[400px] overflow-y-auto sm:w-[540px]">
        <SheetHeader>
          <SheetTitle>Spatialwalk Avatar</SheetTitle>
          <SheetDescription>
            Configure the Spatialwalk Avatar settings for your chat experience.
          </SheetDescription>
        </SheetHeader>

        <div className="my-4">
          <SpatialwalkCfgForm
            initialData={{
              enable_spatialwalk_avatar: spatialwalkSettings.enabled,
              spatialwalk_avatar_id: spatialwalkSettings.avatarId,
              spatialwalk_app_id: spatialwalkSettings.appId,
              spatialwalk_env: spatialwalkSettings.environment,
              spatialwalk_large_window:
                spatialwalkSettings.avatarDesktopLargeWindow,
            }}
            onUpdate={async (data) => {
              if (data.enable_spatialwalk_avatar === true) {
                if (!data.spatialwalk_avatar_id) {
                  toast.error("Spatialwalk Settings", {
                    description: "Please provide an Avatar ID",
                  });
                  return;
                }
                if (!data.spatialwalk_app_id) {
                  toast.error("Spatialwalk Settings", {
                    description: "Please provide an App ID",
                  });
                  return;
                }
              }
              dispatch(
                setSpatialwalkSettings({
                  enabled: data.enable_spatialwalk_avatar as boolean,
                  avatarId: data.spatialwalk_avatar_id as string,
                  appId: data.spatialwalk_app_id as string,
                  environment: (data.spatialwalk_env as "cn" | "intl") || "intl",
                  avatarDesktopLargeWindow:
                    data.spatialwalk_large_window as boolean,
                })
              );
              toast.success("Spatialwalk Settings", {
                description: "Settings updated successfully",
              });
            }}
          />
        </div>
      </SheetContent>
    </Sheet>
  );
}

const SpatialwalkCfgForm = ({
  initialData,
  onUpdate,
}: {
  initialData: Record<string, string | boolean | null | undefined>;
  onUpdate: (data: Record<string, string | boolean | null>) => void;
}) => {
  const formSchema = z.record(
    z.string(),
    z.union([z.string(), z.boolean(), z.null()])
  );
  const form = useForm<z.infer<typeof formSchema>>({
    resolver: zodResolver(formSchema),
    defaultValues: initialData,
  });
  const { watch } = form;
  const enableSpatialwalkAvatar = watch("enable_spatialwalk_avatar");

  const onSubmit = (data: z.infer<typeof formSchema>) => {
    onUpdate(data);
  };

  return (
    <Form {...form}>
      <form onSubmit={form.handleSubmit(onSubmit)} className="space-y-6">
        <FormField
          key={"enable_spatialwalk_avatar"}
          control={form.control}
          name={"enable_spatialwalk_avatar"}
          render={({ field }) => (
            <FormItem>
              <FormLabel>Enable Spatialwalk Avatar</FormLabel>
              <div className="flex items-center justify-between">
                <FormControl>
                  <div className="flex items-center space-x-2">
                    <Switch
                      checked={field.value === true}
                      onCheckedChange={field.onChange}
                    />
                  </div>
                </FormControl>
              </div>
            </FormItem>
          )}
        />
        {enableSpatialwalkAvatar && (
          <>
            <FormField
              key={"spatialwalk_avatar_id"}
              control={form.control}
              name={"spatialwalk_avatar_id"}
              render={({ field }) => (
                <FormItem>
                  <FormLabel>Avatar ID</FormLabel>
                  <div className="flex items-center justify-between">
                    <FormControl>
                      <Input
                        {...field}
                        value={
                          field.value === null || field.value === undefined
                            ? ""
                            : field.value.toString()
                        }
                        type={"text"}
                      />
                    </FormControl>
                  </div>
                </FormItem>
              )}
            />
            <FormField
              key={"spatialwalk_app_id"}
              control={form.control}
              name={"spatialwalk_app_id"}
              render={({ field }) => (
                <FormItem>
                  <FormLabel>AvatarKit App ID</FormLabel>
                  <div className="flex items-center justify-between">
                    <FormControl>
                      <Input
                        {...field}
                        value={
                          field.value === null || field.value === undefined
                            ? ""
                            : field.value.toString()
                        }
                        type={"text"}
                      />
                    </FormControl>
                  </div>
                </FormItem>
              )}
            />
            <FormField
              key={"spatialwalk_env"}
              control={form.control}
              name={"spatialwalk_env"}
              render={({ field }) => (
                <FormItem>
                  <FormLabel>Environment</FormLabel>
                  <FormControl>
                    <Select
                      value={
                        field.value === null || field.value === undefined
                          ? "cn"
                          : field.value.toString()
                      }
                      onValueChange={field.onChange}
                    >
                      <SelectTrigger>
                        <SelectValue placeholder="Select environment" />
                      </SelectTrigger>
                      <SelectContent>
                        <SelectItem value="cn">CN</SelectItem>
                        <SelectItem value="intl">Intl</SelectItem>
                      </SelectContent>
                    </Select>
                  </FormControl>
                </FormItem>
              )}
            />
            <FormField
              key={"spatialwalk_large_window"}
              control={form.control}
              name={"spatialwalk_large_window"}
              render={({ field }) => (
                <FormItem>
                  <FormLabel>Large Window</FormLabel>
                  <div className="flex items-center justify-between">
                    <FormControl>
                      <div className="flex items-center space-x-2">
                        <Switch
                          checked={field.value === true}
                          onCheckedChange={field.onChange}
                        />
                      </div>
                    </FormControl>
                  </div>
                </FormItem>
              )}
            />
          </>
        )}
        <Button type="submit" disabled={form.formState.isSubmitting}>
          {form.formState.isSubmitting ? (
            <>
              <LoaderCircleIcon className="h-4 w-4 animate-spin" />
              <span>Saving...</span>
            </>
          ) : (
            "Save changes"
          )}
        </Button>
      </form>
    </Form>
  );
};
