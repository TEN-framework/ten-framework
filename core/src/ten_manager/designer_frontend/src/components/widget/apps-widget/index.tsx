//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
/* eslint-disable react-hooks/exhaustive-deps */

import { zodResolver } from "@hookform/resolvers/zod";
import { FolderIcon } from "lucide-react";
import * as React from "react";
import { useForm } from "react-hook-form";
import { useTranslation } from "react-i18next";
import { toast } from "sonner";
import { z } from "zod";
import {
  postCreateApp,
  retrieveTemplatePkgs,
  useFetchApps,
} from "@/api/services/apps";
import { AppFileManager } from "@/components/file-manager/app-folder";
import { SpinnerLoading } from "@/components/status/loading";
import { Button } from "@/components/ui/button";
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
  DialogTrigger,
} from "@/components/ui/dialog";
import {
  Form,
  FormControl,
  FormDescription,
  FormField,
  FormItem,
  FormLabel,
  FormMessage,
} from "@/components/ui/form";
import { Input } from "@/components/ui/input";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import { cn } from "@/lib/utils";
import {
  AppCreateReqSchema,
  ETemplateLanguage,
  ETemplateType,
  TemplatePkgsReqSchema,
} from "@/types/apps";

// eslint-disable-next-line max-len
export { AppsManagerWidget } from "@/components/widget/apps-widget/apps-manager";

export const AppTemplateWidget = (props: {
  className?: string;
  onCreated?: (baseDir: string) => void;
}) => {
  const { className, onCreated } = props;
  const [templatePkgs, setTemplatePkgs] = React.useState<
    Record<string, { pkg_name: string; pkg_version: string }[]>
  >({});
  const [showAppFolder, setShowAppFolder] = React.useState<boolean>(false);
  const [isCreating, setIsCreating] = React.useState<boolean>(false);

  const { t } = useTranslation();
  const { mutate: mutateApps } = useFetchApps();

  const formSchema = z.object({
    ...TemplatePkgsReqSchema.shape,
    ...AppCreateReqSchema.shape,
  });

  const form = useForm<z.infer<typeof formSchema>>({
    resolver: zodResolver(formSchema),
    defaultValues: {
      pkg_type: ETemplateType.APP,
      language: ETemplateLanguage.CPP,
      base_dir: undefined,
      app_name: undefined,
      template_name: undefined,
    },
  });

  const onSubmit = async (values: z.infer<typeof formSchema>) => {
    try {
      setIsCreating(true);
      const templateName = values.template_name;
      const res = await postCreateApp({
        base_dir: values.base_dir,
        app_name: values.app_name,
        template_name: templateName,
        template_version: values.template_version,
      });
      toast.success(t("popup.apps.createAppSuccess"), {
        description: res?.app_path || values.base_dir,
      });
      mutateApps();
      onCreated?.(res?.app_path || values.base_dir);
    } catch (error) {
      console.error(error);
      toast.error(t("popup.apps.createAppFailed"), {
        description:
          error instanceof Error
            ? error.message
            : t("popup.apps.createAppFailed"),
      });
    } finally {
      setIsCreating(false);
    }
  };

  // biome-ignore lint/correctness/useExhaustiveDependencies: <ignore>
  React.useEffect(() => {
    const fetchTemplatePkgs = async () => {
      const key = `${form.watch("pkg_type")}-${form.watch("language")}`;
      const existingPkgs = templatePkgs[key];
      if (!existingPkgs || existingPkgs.length === 0) {
        const pkgs = await retrieveTemplatePkgs(
          form.watch("pkg_type"),
          form.watch("language")
        );
        setTemplatePkgs((prev) => ({ ...prev, [key]: pkgs.templates }));
      }
    };
    fetchTemplatePkgs();
  }, [form.watch("pkg_type"), form.watch("language")]);

  return (
    <Form {...form}>
      <form
        onSubmit={form.handleSubmit(onSubmit)}
        className={cn(
          "space-y-4 px-2",
          "max-h-[calc(90dvh-10rem)] overflow-y-auto",
          className
        )}
      >
        <FormField
          control={form.control}
          name="pkg_type"
          render={({ field }) => (
            <FormItem>
              <FormLabel>{t("popup.apps.templateType")}</FormLabel>
              <Select onValueChange={field.onChange} defaultValue={field.value}>
                <FormControl>
                  <SelectTrigger className="w-full max-w-sm">
                    <SelectValue placeholder={t("popup.apps.templateType")} />
                  </SelectTrigger>
                </FormControl>
                <SelectContent className="w-full max-w-sm">
                  {Object.values(ETemplateType).map((type) => (
                    <SelectItem key={type} value={type}>
                      {type}
                    </SelectItem>
                  ))}
                </SelectContent>
              </Select>
              <FormDescription>
                {t("popup.apps.templateTypeDescription")}
              </FormDescription>
              <FormMessage />
            </FormItem>
          )}
        />
        <FormField
          control={form.control}
          name="language"
          render={({ field }) => (
            <FormItem>
              <FormLabel>{t("popup.apps.templateLanguage")}</FormLabel>
              <Select onValueChange={field.onChange} defaultValue={field.value}>
                <FormControl>
                  <SelectTrigger className="w-full max-w-sm">
                    <SelectValue
                      placeholder={t("popup.apps.templateLanguage")}
                    />
                  </SelectTrigger>
                </FormControl>
                <SelectContent className="w-full max-w-sm">
                  {Object.values(ETemplateLanguage).map((language) => (
                    <SelectItem key={language} value={language}>
                      {language}
                    </SelectItem>
                  ))}
                </SelectContent>
              </Select>
              <FormDescription>
                {t("popup.apps.templateLanguageDescription")}
              </FormDescription>
              <FormMessage />
            </FormItem>
          )}
        />
        <FormField
          control={form.control}
          name="template_name"
          render={({ field }) => (
            <FormItem>
              <FormLabel>{t("popup.apps.templateName")}</FormLabel>
              <Select
                onValueChange={field.onChange}
                defaultValue={field.value}
                disabled={
                  !templatePkgs[
                    `${form.watch("pkg_type")}-${form.watch("language")}`
                  ]
                }
              >
                <FormControl>
                  <SelectTrigger className="w-full max-w-sm">
                    <SelectValue
                      placeholder={t("popup.apps.templateLanguage")}
                    />
                  </SelectTrigger>
                </FormControl>
                <SelectContent className="w-full max-w-sm">
                  {Array.from(
                    new Set(
                      templatePkgs[
                        `${form.watch("pkg_type")}-${form.watch("language")}`
                      ]?.map((pkg) => pkg.pkg_name)
                    )
                  ).map((pkgName) => (
                    <SelectItem key={pkgName} value={pkgName}>
                      {pkgName}
                    </SelectItem>
                  ))}
                </SelectContent>
              </Select>
              <FormDescription>
                {t("popup.apps.templateLanguageDescription")}
              </FormDescription>
              <FormMessage />
            </FormItem>
          )}
        />
        {form.watch("template_name") && (
          <FormField
            control={form.control}
            name="template_version"
            render={({ field }) => (
              <FormItem>
                <FormLabel>{t("popup.apps.templateVersion")}</FormLabel>
                <Select
                  onValueChange={field.onChange}
                  defaultValue={field.value}
                >
                  <FormControl>
                    <SelectTrigger className="w-full max-w-sm">
                      <SelectValue
                        placeholder={t("popup.apps.templateVersion")}
                      />
                    </SelectTrigger>
                  </FormControl>
                  <SelectContent className="w-full max-w-sm">
                    {templatePkgs[
                      `${form.watch("pkg_type")}-${form.watch("language")}`
                    ]
                      ?.filter(
                        (pkg) => pkg.pkg_name === form.watch("template_name")
                      )
                      .map((pkg) => (
                        <SelectItem
                          key={pkg.pkg_version + pkg.pkg_name}
                          value={pkg.pkg_version}
                        >
                          {pkg.pkg_version}
                        </SelectItem>
                      ))}
                  </SelectContent>
                </Select>
                <FormDescription>
                  {t("popup.apps.templateVersionDescription")}
                </FormDescription>
                <FormMessage />
              </FormItem>
            )}
          />
        )}
        <FormField
          control={form.control}
          name="app_name"
          render={({ field }) => (
            <FormItem>
              <FormLabel>{t("popup.apps.appName")}</FormLabel>
              <Input {...field} />
              <FormMessage />
            </FormItem>
          )}
        />
        <FormField
          control={form.control}
          name="base_dir"
          render={({ field }) => (
            <FormItem>
              <FormLabel>{t("popup.apps.baseDir")}</FormLabel>
              <Dialog open={showAppFolder} onOpenChange={setShowAppFolder}>
                <DialogTrigger asChild>
                  <Button
                    variant="outline"
                    size="sm"
                    className="w-full max-w-sm"
                  >
                    <span className="overflow-hidden text-ellipsis">
                      {field.value || t("action.chooseBaseDir")}
                    </span>
                    <FolderIcon className="h-4 w-4" />
                  </Button>
                </DialogTrigger>
                <DialogContent className="h-fit w-fit max-w-screen">
                  <DialogHeader>
                    <DialogTitle>{t("popup.apps.baseDir")}</DialogTitle>
                    <DialogDescription>
                      {t("popup.apps.baseDirDescription")}
                    </DialogDescription>
                  </DialogHeader>
                  <AppFileManager
                    className="h-[400px] w-[600px]"
                    onSave={(folderPath) => {
                      field.onChange(folderPath);
                      setShowAppFolder(false);
                    }}
                  />
                </DialogContent>
              </Dialog>
              <FormMessage />
            </FormItem>
          )}
        />
        <Button
          type="submit"
          size="sm"
          disabled={!form.formState.isValid || isCreating}
          className="w-full"
        >
          {isCreating && <SpinnerLoading className="size-4" />}
          {t("action.create")}
        </Button>
      </form>
    </Form>
  );
};
