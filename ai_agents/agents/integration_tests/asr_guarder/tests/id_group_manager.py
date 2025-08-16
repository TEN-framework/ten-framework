#!/usr/bin/env python3
"""
ID Group Manager for ASR Results

This module provides functionality to group ASR results by their IDs.
Each group contains multiple non-final results and one final result.
Groups are separated by final results.
"""

from typing import Dict, List, Optional, Any
from dataclasses import dataclass, field


@dataclass
class AsrResultGroup:
    """Represents a group of ASR results with the same ID."""

    group_id: str
    non_final_results: List[Dict[str, Any]] = field(default_factory=list)
    final_result: Optional[Dict[str, Any]] = None

    def add_non_final(self, result: Dict[str, Any]) -> None:
        """Add a non-final result to this group."""
        self.non_final_results.append(result)

    def set_final(self, result: Dict[str, Any]) -> None:
        """Set the final result for this group."""
        self.final_result = result

    def is_complete(self) -> bool:
        """Check if this group has a final result."""
        return self.final_result is not None

    def get_group_size(self) -> int:
        """Get the total number of results in this group."""
        size = len(self.non_final_results)
        if self.final_result:
            size += 1
        return size


class AsrIdGroupManager:
    """Manages ASR result groups by ID."""

    def __init__(self):
        self.groups: Dict[str, AsrResultGroup] = {}
        self.current_group_id: Optional[str] = None

    def add_result(self, result: Dict[str, Any]) -> str:
        """
        Add an ASR result to the appropriate group.

        Args:
            result: ASR result dictionary

        Returns:
            The group ID this result was added to
        """
        result_id = result.get("id", "")
        is_final = result.get("final", False)

        if not result_id:
            # If no ID, create a new group
            result_id = f"group_{len(self.groups)}"

        # Create group if it doesn't exist
        if result_id not in self.groups:
            self.groups[result_id] = AsrResultGroup(group_id=result_id)

        group = self.groups[result_id]

        if is_final:
            # Final result completes the group
            group.set_final(result)
            self.current_group_id = None
        else:
            # Non-final result adds to current group
            group.add_non_final(result)
            self.current_group_id = result_id

        return result_id

    def get_complete_groups(self) -> List[AsrResultGroup]:
        """Get all groups that have a final result."""
        return [group for group in self.groups.values() if group.is_complete()]

    def get_incomplete_groups(self) -> List[AsrResultGroup]:
        """Get all groups that don't have a final result."""
        return [
            group for group in self.groups.values() if not group.is_complete()
        ]

    def get_group_by_id(self, group_id: str) -> Optional[AsrResultGroup]:
        """Get a specific group by ID."""
        return self.groups.get(group_id)

    def get_total_groups(self) -> int:
        """Get the total number of groups."""
        return len(self.groups)

    def get_complete_groups_count(self) -> int:
        """Get the number of complete groups."""
        return len(self.get_complete_groups())

    def validate_group_consistency(self) -> bool:
        """
        Validate that all groups have consistent structure.

        Returns:
            True if all groups are consistent, False otherwise
        """
        for group in self.groups.values():
            if group.final_result:
                # Check that final result has the same ID as group
                if group.final_result.get("id", "") != group.group_id:
                    return False

                # Check that all non-final results have the same ID
                for non_final in group.non_final_results:
                    if non_final.get("id", "") != group.group_id:
                        return False

        return True

    def get_group_summary(self) -> Dict[str, Any]:
        """Get a summary of all groups."""
        return {
            "total_groups": self.get_total_groups(),
            "complete_groups": self.get_complete_groups_count(),
            "incomplete_groups": len(self.get_incomplete_groups()),
            "groups": {
                group_id: {
                    "non_final_count": len(group.non_final_results),
                    "has_final": group.final_result is not None,
                    "total_results": group.get_group_size(),
                }
                for group_id, group in self.groups.items()
            },
        }
