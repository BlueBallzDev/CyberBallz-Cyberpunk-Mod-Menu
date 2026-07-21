"""YouTube Data API v3 upload (videos.insert + thumbnails.set)."""

from pathlib import Path

from google.auth.transport.requests import Request
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build
from googleapiclient.http import MediaFileUpload

from .config import ROOT, STATE_DIR

SCOPES = ["https://www.googleapis.com/auth/youtube.upload"]
CLIENT_SECRET = ROOT / "client_secret.json"
TOKEN_FILE = STATE_DIR / "token.json"


def run_auth_flow() -> None:
    """One-time interactive OAuth. Needs a browser; run on a desktop, then copy
    state/token.json to the machine that does the uploading."""
    if not CLIENT_SECRET.exists():
        raise SystemExit(
            f"Missing {CLIENT_SECRET}. Create an OAuth client (Desktop app) in "
            "Google Cloud Console and download it there. See README.md."
        )
    flow = InstalledAppFlow.from_client_secrets_file(str(CLIENT_SECRET), SCOPES)
    creds = flow.run_local_server(port=0)
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    TOKEN_FILE.write_text(creds.to_json())
    print(f"Saved credentials to {TOKEN_FILE}")


def get_service():
    if not TOKEN_FILE.exists():
        raise SystemExit("Not authenticated. Run: python run.py auth")
    creds = Credentials.from_authorized_user_file(str(TOKEN_FILE), SCOPES)
    if creds.expired and creds.refresh_token:
        creds.refresh(Request())
        TOKEN_FILE.write_text(creds.to_json())
    return build("youtube", "v3", credentials=creds)


def upload_video(
    video_path: Path,
    title: str,
    description: str,
    tags: list[str],
    category_id: str,
    privacy: str,
    publish_at: str | None,
    notify_subscribers: bool,
    thumbnail_path: Path | None = None,
) -> str:
    service = get_service()

    status: dict = {
        "privacyStatus": privacy,
        "selfDeclaredMadeForKids": False,
    }
    if publish_at:
        status["privacyStatus"] = "private"
        status["publishAt"] = publish_at

    body = {
        "snippet": {
            "title": title,
            "description": description,
            "tags": tags,
            "categoryId": category_id,
        },
        "status": status,
    }

    media = MediaFileUpload(str(video_path), mimetype="video/mp4",
                            chunksize=-1, resumable=True)
    request = service.videos().insert(
        part="snippet,status",
        body=body,
        media_body=media,
        notifySubscribers=notify_subscribers,
    )

    response = None
    while response is None:
        progress, response = request.next_chunk()
        if progress:
            print(f"  upload {int(progress.progress() * 100)}%")
    video_id = response["id"]
    print(f"  uploaded: https://youtu.be/{video_id}")

    if thumbnail_path is not None:
        # Custom thumbnails need a phone-verified channel; don't fail the run over it.
        try:
            service.thumbnails().set(
                videoId=video_id,
                media_body=MediaFileUpload(str(thumbnail_path), mimetype="image/png"),
            ).execute()
            print("  thumbnail set")
        except Exception as e:
            print(f"  thumbnail failed (channel not verified for custom thumbnails?): {e}")

    return video_id
