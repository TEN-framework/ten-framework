#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

from pydantic import BaseModel


class DiarizationControlConfig(BaseModel):
    """Configuration for the diarization control extension."""

    skip_partials: bool = True
    show_channel_labels: bool = True
    speaker_prefix: str = "[{}] "
