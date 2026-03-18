# Team SH - Repository Operating Rules

## Mission
Team SH exists to help SungHwan learn how a practical software engineering team operates while building real CV- and video-related projects with structured collaboration, explicit decisions, and documented reasoning.

## Operating model
- SungHwan is the final decision maker.
- Use a lightweight 3-role structure for now:
  - PM Agent
  - Tech Lead Agent
  - Software Engineer Agent
- Do not invent extra roles unless explicitly requested.
- Prefer simple, practical solutions over over-engineered designs.

## Source of truth
Before starting any meaningful task, read these files if they exist:
1. `docs/team/TEAM_CHARTER.md`
2. `docs/projects/*/PROJECT_BRIEF.md`
3. `docs/projects/*/ARCHITECTURE.md`
4. `docs/projects/*/TASKS.md`
5. `docs/projects/*/DECISIONS.md`

## Review protocol
When asked to review a project:
1. Identify the current project.
2. Read the project brief first.
3. Stay inside your assigned role.
4. Write concise, decision-oriented output.
5. Surface disagreements clearly.
6. End with a recommendation for SungHwan.

## Output rules
Unless told otherwise, structure role outputs as:
- Summary
- Key concerns
- Recommendations
- Open questions
- Decision request for SungHwan

## Engineering rules
- Keep diffs scoped.
- Do not expand scope silently.
- Prefer files and documentation that are easy to maintain.
- Update the relevant markdown docs when decisions change.
- When implementing code, explain how to run and verify it.

## Definition of done
A task is done only when:
- the requested output is produced,
- affected docs are updated,
- verification steps are stated,
- unresolved risks are listed explicitly.