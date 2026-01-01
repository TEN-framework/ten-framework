"use client";

import NextLink from "next/link";
import { useRouter } from "next/navigation";
import type React from "react";
import { useEffect, useState } from "react";
import { toast } from "sonner";
import {
  GITHUB_URL,
  getRandomChannel,
  getRandomUserId,
  useAppDispatch,
} from "@/common";
import { LoadingButton } from "@/components/Button/LoadingButton";
import { GitHubIcon } from "@/components/Icon";
import { Button } from "@/components/ui/button";
import {
  Card,
  CardContent,
  CardDescription,
  CardFooter,
  CardHeader,
  CardTitle,
} from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { setOptions } from "@/store/reducers/global";
import packageData from "../../../package.json";

const { version } = packageData;

export default function LoginCard() {
  const dispatch = useAppDispatch();
  const router = useRouter();
  const [userName, setUserName] = useState("");
  const [isLoadingSuccess, setIsLoadingSuccess] = useState(false);

  const onUserNameChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    let value = e.target.value;
    value = value.replace(/\s/g, "");
    setUserName(value);
  };

  useEffect(() => {
    const onPageLoad = () => {
      setIsLoadingSuccess(true);
    };

    if (document.readyState === "complete") {
      onPageLoad();
    } else {
      window.addEventListener("load", onPageLoad, false);
      return () => window.removeEventListener("load", onPageLoad);
    }
  }, []);

  const onClickJoin = () => {
    if (!userName) {
      toast.error("please enter a user name");
      return;
    }
    const userId = getRandomUserId();
    dispatch(
      setOptions({
        userName,
        channel: getRandomChannel(),
        userId,
      })
    );
    router.push("/home");
  };

  return (
    <div className="flex h-full w-full items-center justify-center overflow-y-auto p-4">
      <Card className="w-full max-w-md rounded-xl border border-[#20272D] bg-gradient-to-br bg-transparent from-[rgba(31,69,141,0.16)] via-[rgba(31,69,141,0)] to-[#1F458D] shadow-[0px_3.999px_48.988px_0px_rgba(0,7,72,0.12)] backdrop-blur-[8.8px]">
        <CardHeader>
          <Button
            asChild
            variant="outline"
            size="sm"
            className="ml-auto w-fit rounded-full bg-transparent px-2 py-1"
          >
            <NextLink href={GITHUB_URL} target="_blank">
              <GitHubIcon className="h-4 w-4" />
              <span className="">GitHub</span>
            </NextLink>
          </Button>
          <CardTitle className="text-center text-2xl md:text-3xl">
            TEN Agent
          </CardTitle>
          <CardDescription className="text-center text-inherit text-md md:text-lg">
            Multi-Purpose Voice Assistant Agent Example Powered by TEN
          </CardDescription>
        </CardHeader>
        <CardContent className="">
          <form
            onSubmit={(e) => {
              e.preventDefault();
              onClickJoin();
            }}
            className="flex flex-col gap-6"
          >
            <Input
              placeholder="User Name"
              value={userName}
              onChange={onUserNameChange}
              disabled={!isLoadingSuccess}
            />
            <LoadingButton
              type="submit"
              disabled={!isLoadingSuccess || !userName}
              loading={!isLoadingSuccess}
            >
              {isLoadingSuccess ? "Join" : "Loading"}
            </LoadingButton>
          </form>
        </CardContent>
        <CardFooter className="flex w-full items-center justify-center text-muted-foreground text-sm">
          <p>Version {version}</p>
        </CardFooter>
      </Card>
    </div>
  );
}
