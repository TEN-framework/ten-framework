import LogoSvg from "@/assets/logo.svg";
import SmallLogoSvg from "@/assets/logo_small.svg";
import type { IconProps } from "../types";

export const LogoIcon = (props: IconProps) => {
  const { size = "default" } = props;
  return size === "small" ? (
    <SmallLogoSvg {...props}></SmallLogoSvg>
  ) : (
    <LogoSvg {...props}></LogoSvg>
  );
};
