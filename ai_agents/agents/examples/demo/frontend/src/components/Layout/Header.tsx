// import { LogoIcon, SmallLogoIcon } from "@/components/Icon"

import { TenLogo } from "@/components/Icon";
import { cn } from "@/lib/utils";
import { HeaderActions } from "./HeaderComponents";

export default function Header(props: { className?: string }) {
  const { className } = props;
  return (
    <>
      {/* Header */}
      <header
        className={cn(
          "flex items-center justify-between bg-[#181a1d] p-2 font-roboto md:p-4",
          className
        )}
      >
        <div className="flex items-center space-x-2">
          <TenLogo className="h-3 md:h-5" />
          {/* <LogoIcon className="hidden h-5 md:block" />
          <SmallLogoIcon className="block h-4 md:hidden" /> */}
          <h1 className="font-bold text-sm md:text-xl">TEN Agent Example</h1>
        </div>
        <HeaderActions />
      </header>
    </>
  );
}
