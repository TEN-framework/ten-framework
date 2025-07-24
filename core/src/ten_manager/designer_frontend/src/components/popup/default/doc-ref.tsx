//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";
import { useTranslation } from "react-i18next";
import { toast } from "sonner";
import { useRetrieveDocLink } from "@/api/services/doc";
import { SpinnerLoading } from "@/components/status/loading";
import { TEN_DOC_URL } from "@/constants";
import type { EDocLinkKey } from "@/types/doc";
import type { IDefaultWidget } from "@/types/widgets";

export const DocRefPopupTitle = (props: { name: string }) => {
  const { name } = props;
  const { t } = useTranslation();

  return t("popup.doc.title", { name });
};

export const DocRefPopupContent = (props: { widget: IDefaultWidget }) => {
  const { widget } = props;
  const { doc_link_key } = widget.metadata;

  const { i18n } = useTranslation();

  if (!doc_link_key) return null;

  return <DocRefRemoteContent locale={i18n.language} queryKey={doc_link_key} />;
};

const DocRefRemoteContent = (props: {
  locale: string;
  queryKey: EDocLinkKey;
}) => {
  const { locale, queryKey } = props;

  const { data, error, isLoading } = useRetrieveDocLink(queryKey, locale);

  const docPathMemo = React.useMemo(() => {
    const text = data?.text;
    if (!text) {
      return undefined;
    }
    return `${text}`;
  }, [data?.text]);

  React.useEffect(() => {
    if (error) {
      toast.error(error.message);
    }
  }, [error]);

  if (isLoading) {
    return <SpinnerLoading className="h-full w-full" />;
  }

  if (!docPathMemo) return null;

  return (
    <iframe
      src={TEN_DOC_URL + docPathMemo}
      className="h-full w-full"
      title={queryKey}
      sandbox="allow-scripts allow-same-origin"
    />
  );
};
