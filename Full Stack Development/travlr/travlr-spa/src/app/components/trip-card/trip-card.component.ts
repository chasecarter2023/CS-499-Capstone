// src/app/components/trip-card/trip-card.component.ts
import { Component, Input, Output, EventEmitter } from '@angular/core';
import { CommonModule } from '@angular/common';
import { RouterModule } from '@angular/router';
import { Trip } from '../../services/trip.service';

@Component({
  selector: 'app-trip-card',
  standalone: true,
  imports: [CommonModule, RouterModule],
  templateUrl: './trip-card.component.html',
  styleUrls: ['./trip-card.component.css']
})
export class TripCardComponent {
  @Input() trip!: Trip;
  @Output() deleteTrip: EventEmitter<string> = new EventEmitter<string>();

  // Method to emit the deletion event
  onDelete(): void {
    this.deleteTrip.emit(this.trip.id);
  }
}
