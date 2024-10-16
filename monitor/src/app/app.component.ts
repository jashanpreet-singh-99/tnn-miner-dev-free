import { Component, OnInit, Renderer2 } from '@angular/core';
import { RouterOutlet } from '@angular/router';
import { QuickBarComponent } from "./components/shared/quick-bar/quick-bar.component";

@Component({
  selector: 'app-root',
  standalone: true,
  imports: [RouterOutlet, QuickBarComponent],
  templateUrl: './app.component.html',
  styleUrl: './app.component.scss'
})
export class AppComponent implements OnInit {
  title = 'monitor';

  ngOnInit(): void {

  }
}
